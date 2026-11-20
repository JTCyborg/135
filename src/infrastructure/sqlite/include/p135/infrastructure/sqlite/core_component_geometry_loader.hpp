#pragma once

#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <optional>
#include <string>

namespace p135::infrastructure::sqlite {

/// Datasheet-derived magnetic geometry for one exact immutable core-component revision.
/// SI units. Mechanical CAD profile is tracked separately and is not inferred from Ae/le.
struct CoreComponentGeometrySnapshot {
    std::string revisionId;
    std::string componentId;
    std::string orderingCode;
    std::string manufacturer;
    std::string coreShape;
    /// Generic material designation; supports ferrites and electrical steels.
    std::string magneticMaterial;
    std::string magneticMaterialFamily;
    /// Legacy compatibility field for existing TDK ferrite component records.
    std::string ferriteMaterial;
    std::string sourceReference;
    std::string datasetRevision;
    std::string geometryStatus;
    std::string cadGeometryStatus;
    std::optional<double> effectiveAreaM2;
    std::optional<double> minimumAreaM2;
    std::optional<double> effectivePathLengthM;
    std::optional<double> effectiveVolumeM3;
    std::optional<double> coreFactorPerM;
    std::optional<double> nominalAlHPerTurnSquared;

    [[nodiscard]] bool hasMagneticGeometry() const noexcept;
    [[nodiscard]] bool hasCompleteCadGeometry() const noexcept;
};

/// Decodes a pinned core-component revision. Missing geometry is preserved as
/// unavailable; malformed or non-positive supplied geometry is rejected.
[[nodiscard]] Result<CoreComponentGeometrySnapshot> loadCoreComponentGeometryRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId);

} // namespace p135::infrastructure::sqlite
