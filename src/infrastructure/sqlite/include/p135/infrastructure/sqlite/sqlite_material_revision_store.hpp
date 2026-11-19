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

/// Immutable calculation snapshot. input/result/engineVersions are canonical JSON.
struct CalculationRecord {
    std::string calculationId;
    std::string projectId;
    std::string projectName;
    std::string productFamily;
    std::string inputJson;
    std::string resultJson;
    std::string engineVersionsJson;
    std::string contentHash;
    std::string author;
    std::string createdAtUtc;
};

struct CalculationMaterialReference {
    std::string usageId;
    std::string revisionId;
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
    [[nodiscard]] Result<bool> isArchived(const std::string& materialId) const;
    [[nodiscard]] Result<std::vector<application::MaterialRevision>>
    revisionHistory(const std::string& materialId) const;
    /// Search material identities with their latest immutable revision; default hides archived rows.
    [[nodiscard]] Result<std::vector<application::MaterialRevision>>
    searchLatestRevisions(const MaterialCatalogFilter& filter) const;
    /// Append a durable exact-revision reference from a saved product or calculation.
    [[nodiscard]] Result<void> recordUsage(const MaterialUsageReference& usage);
    /// Atomically append an immutable calculation snapshot and exact material revision links.
    [[nodiscard]] Result<void> recordCalculation(
        const CalculationRecord& calculation,
        const std::vector<CalculationMaterialReference>& materials);
    [[nodiscard]] Result<std::optional<CalculationRecord>>
    findCalculation(const std::string& calculationId) const;
    [[nodiscard]] Result<std::vector<CalculationRecord>>
    listCalculations(const std::string& searchText = {},
                     std::uint32_t pageSize = 100,
                     std::uint64_t pageOffset = 0) const;
    [[nodiscard]] Result<std::vector<CalculationMaterialReference>>
    calculationMaterials(const std::string& calculationId) const;


private:
    SqliteDatabase& database_;
};

} // namespace p135::infrastructure::sqlite
