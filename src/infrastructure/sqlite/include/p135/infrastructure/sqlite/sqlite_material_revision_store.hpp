#pragma once

#include "p135/application/material_revision_use_cases.hpp"
#include "p135/infrastructure/sqlite/sqlite_database.hpp"

#include <vector>

namespace p135::infrastructure::sqlite {

enum class MaterialUsageKind { Product, Calculation };

struct MaterialCatalogFilter {
    std::string recordType;
    std::string searchText;
    bool includeArchived{false};
    std::uint32_t pageSize{100};
    std::uint64_t pageOffset{};
};

struct MaterialUsageReference {
    std::string usageId;
    std::string revisionId;
    MaterialUsageKind kind{MaterialUsageKind::Product};
    std::string targetId;
    std::string createdAtUtc;
    std::string actor;
};

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
    [[nodiscard]] Result<std::vector<application::MaterialRevision>>
    revisionHistory(const std::string& materialId) const;
    /// Search material identities with their latest immutable revision; default hides archived rows.
    [[nodiscard]] Result<std::vector<application::MaterialRevision>>
    searchLatestRevisions(const MaterialCatalogFilter& filter) const;
    /// Append a durable exact-revision reference from a saved product or calculation.
    [[nodiscard]] Result<void> recordUsage(const MaterialUsageReference& usage);

private:
    SqliteDatabase& database_;
};

} // namespace p135::infrastructure::sqlite
