#pragma once

#include "p135/application/material_revision_use_cases.hpp"
#include "p135/infrastructure/sqlite/sqlite_database.hpp"

namespace p135::infrastructure::sqlite {

/// SQLite-backed immutable material revision store and exact-revision usage query.
class SqliteMaterialRevisionStore final
    : public application::MaterialRevisionRepository,
      public application::MaterialUsageQuery {
public:
    explicit SqliteMaterialRevisionStore(SqliteDatabase& database) : database_(database) {}

    [[nodiscard]] Result<std::optional<application::MaterialRevision>>
    findRevision(const std::string& revisionId) const override;
    [[nodiscard]] Result<std::optional<application::MaterialRevision>>
    latestRevision(const std::string& materialId) const override;
    [[nodiscard]] Result<void>
    appendRevision(const application::MaterialRevision& revision) override;
    [[nodiscard]] Result<void> setArchived(const std::string& materialId,
                                           bool archived,
                                           const std::string& actor,
                                           const std::string& timestampUtc) override;
    [[nodiscard]] Result<bool> isMaterialInUse(const std::string& materialId) const override;

private:
    SqliteDatabase& database_;
};

} // namespace p135::infrastructure::sqlite
