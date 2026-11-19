#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <openssl/evp.h>
#include <sqlite3.h>

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace p135::infrastructure::sqlite {
namespace {
using application::MaterialRevision;

Error storeError(const std::string& message, ErrorCode code = ErrorCode::IoFailure)
{
    Error error;
    error.code = code;
    error.message = message;
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "SQLITE_MATERIAL_STORE", message,
        std::nullopt, std::nullopt, std::nullopt
    });
    return error;
}

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db)
    {
        if (db_ && sqlite3_prepare_v2(db_, sql, -1, &statement_, nullptr) != SQLITE_OK)
            statement_ = nullptr;
    }
    ~Statement() { if (statement_) sqlite3_finalize(statement_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    sqlite3_stmt* get() const noexcept { return statement_; }
    void finalize() noexcept { if (statement_) { sqlite3_finalize(statement_); statement_ = nullptr; } }
    explicit operator bool() const noexcept { return statement_ != nullptr; }
    std::string error() const { return db_ ? sqlite3_errmsg(db_) : "Database is not open."; }
private:
    sqlite3* db_{};
    sqlite3_stmt* statement_{};
};

class Transaction {
public:
    explicit Transaction(SqliteDatabase& db) : db_(db), active_(static_cast<bool>(db_.beginImmediate())) {}
    ~Transaction() { if (active_) (void)db_.rollback(); }
    bool active() const noexcept { return active_; }
    Result<void> commit()
    {
        auto result = db_.commit();
        if (result) active_ = false;
        return result;
    }
private:
    SqliteDatabase& db_;
    bool active_{};
};

std::string columnText(sqlite3_stmt* statement, int column)
{
    const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
    return value ? value : "";
}

std::optional<MaterialRevision> readRevision(sqlite3_stmt* statement)
{
    MaterialRevision value;
    value.revisionId = columnText(statement, 0);
    value.materialId = columnText(statement, 1);
    value.revisionNumber = static_cast<std::uint64_t>(sqlite3_column_int64(statement, 2));
    if (sqlite3_column_type(statement, 3) != SQLITE_NULL)
        value.parentRevisionId = columnText(statement, 3);
    value.recordType = columnText(statement, 4);
    value.payload = columnText(statement, 5);
    value.contentHash = columnText(statement, 6);
    value.sourceReference = columnText(statement, 7);
    value.datasetRevision = columnText(statement, 8);
    value.author = columnText(statement, 9);
    value.createdAtUtc = columnText(statement, 10);
    return value;
}

Result<std::string> payloadHash(const std::string& payload)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(payload.data(), payload.size(), digest.data(), &length,
                   EVP_sha256(), nullptr) != 1 || length != 32)
        return Result<std::string>::failure(storeError("Unable to calculate snapshot SHA-256."));
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i)
        output << std::setw(2) << static_cast<unsigned int>(digest[i]);
    return Result<std::string>::success(output.str());
}

constexpr const char* kRevisionColumns =
    "revision_id,material_id,revision_number,parent_revision_id,record_type,"
    "payload_json,content_hash,source_reference,dataset_revision,author,created_at_utc";
}

Result<std::optional<MaterialRevision>>
SqliteMaterialRevisionStore::findRevision(const std::string& revisionId) const
{
    Statement query(database_.handle(),
        "SELECT revision_id,material_id,revision_number,parent_revision_id,record_type,"
        "payload_json,content_hash,source_reference,dataset_revision,author,created_at_utc "
        "FROM material_revisions WHERE revision_id=?1;");
    if (!query) return Result<std::optional<MaterialRevision>>::failure(storeError(query.error()));
    sqlite3_bind_text(query.get(), 1, revisionId.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(query.get());
    if (rc == SQLITE_DONE)
        return Result<std::optional<MaterialRevision>>::success(std::nullopt);
    if (rc != SQLITE_ROW)
        return Result<std::optional<MaterialRevision>>::failure(storeError(query.error()));
    return Result<std::optional<MaterialRevision>>::success(readRevision(query.get()));
}

Result<std::optional<MaterialRevision>>
SqliteMaterialRevisionStore::latestRevision(const std::string& materialId) const
{
    const std::string sql = std::string("SELECT ") + kRevisionColumns +
        " FROM material_revisions WHERE material_id=?1 ORDER BY revision_number DESC LIMIT 1;";
    Statement query(database_.handle(), sql.c_str());
    if (!query) return Result<std::optional<MaterialRevision>>::failure(storeError(query.error()));
    sqlite3_bind_text(query.get(), 1, materialId.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(query.get());
    if (rc == SQLITE_DONE)
        return Result<std::optional<MaterialRevision>>::success(std::nullopt);
    if (rc != SQLITE_ROW)
        return Result<std::optional<MaterialRevision>>::failure(storeError(query.error()));
    return Result<std::optional<MaterialRevision>>::success(readRevision(query.get()));
}

Result<void> SqliteMaterialRevisionStore::appendRevision(const MaterialRevision& revision)
{
    if (revision.revisionId.empty() || revision.materialId.empty() || revision.recordType.empty() ||
        revision.payload.empty() || revision.contentHash.empty() || revision.sourceReference.empty() ||
        revision.datasetRevision.empty() || revision.author.empty() || revision.createdAtUtc.empty() ||
        revision.revisionNumber == 0)
        return Result<void>::failure(storeError("Revision metadata and payload must be complete.",
                                                ErrorCode::MissingRequiredData));
    const auto calculatedHash = payloadHash(revision.payload);
    if (!calculatedHash) return Result<void>::failure(calculatedHash.error());
    if (calculatedHash.value() != revision.contentHash)
        return Result<void>::failure(storeError("Snapshot content hash does not match payload.",
                                                ErrorCode::InvalidInput));

    Transaction transaction(database_);
    if (!transaction.active())
        return Result<void>::failure(storeError("Cannot begin material revision transaction."));

    Statement material(database_.handle(),
        "INSERT INTO materials(material_id,record_type,created_at_utc,modified_at_utc) "
        "VALUES(?1,?2,?3,?3) ON CONFLICT(material_id) DO NOTHING;");
    if (!material) return Result<void>::failure(storeError(material.error()));
    sqlite3_bind_text(material.get(), 1, revision.materialId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(material.get(), 2, revision.recordType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(material.get(), 3, revision.createdAtUtc.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(material.get()) != SQLITE_DONE)
        return Result<void>::failure(storeError(material.error()));

    Statement identity(database_.handle(),
        "SELECT record_type,archived FROM materials WHERE material_id=?1;");
    if (!identity) return Result<void>::failure(storeError(identity.error()));
    sqlite3_bind_text(identity.get(), 1, revision.materialId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(identity.get()) != SQLITE_ROW)
        return Result<void>::failure(storeError(identity.error()));
    if (columnText(identity.get(), 0) != revision.recordType)
        return Result<void>::failure(storeError("Record type cannot change between revisions.",
                                                ErrorCode::InvalidInput));
    if (sqlite3_column_int(identity.get(), 1) != 0)
        return Result<void>::failure(storeError("Archived material cannot receive a new revision.",
                                                ErrorCode::InvalidInput));
    identity.finalize();

    Statement insert(database_.handle(),
        "INSERT INTO material_revisions(revision_id,material_id,revision_number,parent_revision_id,"
        "record_type,payload_json,content_hash,source_reference,dataset_revision,author,created_at_utc) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11);");
    if (!insert) return Result<void>::failure(storeError(insert.error()));
    sqlite3_bind_text(insert.get(), 1, revision.revisionId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 2, revision.materialId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(insert.get(), 3, static_cast<sqlite3_int64>(revision.revisionNumber));
    if (revision.parentRevisionId)
        sqlite3_bind_text(insert.get(), 4, revision.parentRevisionId->c_str(), -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(insert.get(), 4);
    sqlite3_bind_text(insert.get(), 5, revision.recordType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 6, revision.payload.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 7, revision.contentHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 8, revision.sourceReference.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 9, revision.datasetRevision.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 10, revision.author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert.get(), 11, revision.createdAtUtc.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(insert.get()) != SQLITE_DONE)
        return Result<void>::failure(storeError(insert.error(), ErrorCode::InvalidInput));

    Statement update(database_.handle(),
        "UPDATE materials SET modified_at_utc=?1 WHERE material_id=?2;");
    if (!update) return Result<void>::failure(storeError(update.error()));
    sqlite3_bind_text(update.get(), 1, revision.createdAtUtc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(update.get(), 2, revision.materialId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(update.get()) != SQLITE_DONE)
        return Result<void>::failure(storeError(update.error()));

    Statement audit(database_.handle(),
        "INSERT INTO audit_log(entity_type,entity_id,event_type,actor,occurred_at_utc,details_json) "
        "VALUES('material',?1,'revision_created',?2,?3,'{}');");
    if (!audit) return Result<void>::failure(storeError(audit.error()));
    sqlite3_bind_text(audit.get(), 1, revision.materialId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(audit.get(), 2, revision.author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(audit.get(), 3, revision.createdAtUtc.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(audit.get()) != SQLITE_DONE)
        return Result<void>::failure(storeError(audit.error()));
    return transaction.commit();
}

Result<void> SqliteMaterialRevisionStore::setArchived(const std::string& materialId,
                                                       bool archived,
                                                       const std::string& actor,
                                                       const std::string& timestampUtc)
{
    if (materialId.empty() || actor.empty() || timestampUtc.empty())
        return Result<void>::failure(storeError("Archive metadata is required.",
                                                ErrorCode::MissingRequiredData));
    Transaction transaction(database_);
    if (!transaction.active())
        return Result<void>::failure(storeError("Cannot begin archive transaction."));
    Statement update(database_.handle(),
        "UPDATE materials SET archived=?1,modified_at_utc=?2 WHERE material_id=?3;");
    if (!update) return Result<void>::failure(storeError(update.error()));
    sqlite3_bind_int(update.get(), 1, archived ? 1 : 0);
    sqlite3_bind_text(update.get(), 2, timestampUtc.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(update.get(), 3, materialId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(update.get()) != SQLITE_DONE)
        return Result<void>::failure(storeError(update.error()));
    if (sqlite3_changes(database_.handle()) != 1)
        return Result<void>::failure(storeError("Material does not exist.", ErrorCode::MissingRequiredData));

    Statement audit(database_.handle(),
        "INSERT INTO audit_log(entity_type,entity_id,event_type,actor,occurred_at_utc,details_json) "
        "VALUES('material',?1,?2,?3,?4,'{}');");
    if (!audit) return Result<void>::failure(storeError(audit.error()));
    sqlite3_bind_text(audit.get(), 1, materialId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(audit.get(), 2, archived ? "archived" : "restored", -1, SQLITE_STATIC);
    sqlite3_bind_text(audit.get(), 3, actor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(audit.get(), 4, timestampUtc.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(audit.get()) != SQLITE_DONE)
        return Result<void>::failure(storeError(audit.error()));
    return transaction.commit();
}

Result<bool> SqliteMaterialRevisionStore::isMaterialInUse(const std::string& materialId) const
{
    Statement query(database_.handle(),
        "SELECT 1 FROM material_usage u "
        "JOIN material_revisions r ON r.revision_id=u.revision_id "
        "WHERE r.material_id=?1 LIMIT 1;");
    if (!query) return Result<bool>::failure(storeError(query.error()));
    sqlite3_bind_text(query.get(), 1, materialId.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(query.get());
    if (rc == SQLITE_ROW) return Result<bool>::success(true);
    if (rc == SQLITE_DONE) return Result<bool>::success(false);
    return Result<bool>::failure(storeError(query.error()));
}

} // namespace p135::infrastructure::sqlite
