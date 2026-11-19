#include "p135/infrastructure/sqlite/migration_runner.hpp"

#include <openssl/evp.h>
#include <sqlite3.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace p135::infrastructure::sqlite {
namespace {
Error migrationError(const std::string& message)
{
    Error error;
    error.code = ErrorCode::IoFailure;
    error.message = message;
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "SQLITE_MIGRATION_FAILED", message,
        std::nullopt, std::nullopt, std::nullopt
    });
    return error;
}

Result<std::string> sha256(const std::string& bytes)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &length,
                   EVP_sha256(), nullptr) != 1 || length != 32) {
        return Result<std::string>::failure(migrationError("OpenSSL SHA-256 failed."));
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i)
        output << std::setw(2) << static_cast<unsigned int>(digest[i]);
    return Result<std::string>::success(output.str());
}

class Transaction {
public:
    explicit Transaction(SqliteDatabase& database) : database_(database)
    {
        active_ = static_cast<bool>(database_.beginImmediate());
    }
    ~Transaction() { if (active_) (void)database_.rollback(); }
    bool active() const noexcept { return active_; }
    Result<void> commit()
    {
        auto result = database_.commit();
        if (result) active_ = false;
        return result;
    }
private:
    SqliteDatabase& database_;
    bool active_{false};
};
}

Result<void> MigrationRunner::apply(const Migration& migration)
{
    if (migration.version <= 0 || migration.name.empty() || migration.file.empty())
        return Result<void>::failure(migrationError("Migration metadata is incomplete."));

    std::ifstream input(migration.file, std::ios::binary);
    if (!input)
        return Result<void>::failure(migrationError("Cannot read migration file: " + migration.file.string()));
    const std::string sql((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (sql.empty())
        return Result<void>::failure(migrationError("Migration file is empty."));

    const auto hash = sha256(sql);
    if (!hash) return Result<void>::failure(hash.error());

    Transaction transaction(database_);
    if (!transaction.active())
        return Result<void>::failure(migrationError("Cannot begin migration transaction."));

    auto schema = database_.execute(sql);
    if (!schema) return schema;

    sqlite3_stmt* query = nullptr;
    const char* selectSql = "SELECT checksum_sha256 FROM schema_migrations WHERE version = ?1;";
    if (sqlite3_prepare_v2(database_.handle(), selectSql, -1, &query, nullptr) != SQLITE_OK)
        return Result<void>::failure(migrationError(sqlite3_errmsg(database_.handle())));
    sqlite3_bind_int(query, 1, migration.version);
    const int queryResult = sqlite3_step(query);
    if (queryResult == SQLITE_ROW) {
        const auto* stored = reinterpret_cast<const char*>(sqlite3_column_text(query, 0));
        const std::string storedHash = stored ? stored : "";
        sqlite3_finalize(query);
        if (storedHash != hash.value())
            return Result<void>::failure(migrationError("Applied migration checksum mismatch."));
        return transaction.commit();
    }
    sqlite3_finalize(query);
    if (queryResult != SQLITE_DONE)
        return Result<void>::failure(migrationError(sqlite3_errmsg(database_.handle())));

    sqlite3_stmt* insert = nullptr;
    const char* insertSql =
        "INSERT INTO schema_migrations(version,name,checksum_sha256,applied_at_utc) "
        "VALUES(?1,?2,?3,strftime('%Y-%m-%dT%H:%M:%fZ','now'));";
    if (sqlite3_prepare_v2(database_.handle(), insertSql, -1, &insert, nullptr) != SQLITE_OK)
        return Result<void>::failure(migrationError(sqlite3_errmsg(database_.handle())));
    sqlite3_bind_int(insert, 1, migration.version);
    sqlite3_bind_text(insert, 2, migration.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert, 3, hash.value().c_str(), -1, SQLITE_TRANSIENT);
    const int insertResult = sqlite3_step(insert);
    sqlite3_finalize(insert);
    if (insertResult != SQLITE_DONE)
        return Result<void>::failure(migrationError(sqlite3_errmsg(database_.handle())));
    return transaction.commit();
}

} // namespace p135::infrastructure::sqlite
