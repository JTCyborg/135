#pragma once

#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <optional>
#include <string>
#include <vector>

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
    /// Outline dimensions for three-phase EI laminations; SI units.
    std::optional<double> columnWidthM;
    std::optional<double> overallHeightM;
    std::optional<double> overallWidthM;
    std::optional<double> windowHeightM;
    std::optional<double> windowWidthM;
    std::optional<double> holePitchM;
    std::optional<double> holeDiameterM;
    std::optional<double> dimensionHM;
    std::optional<double> dimensionNM;
    std::optional<double> sourceWeightGramsPer10mmStack;
    std::optional<int> holeCount;
    std::optional<double> stackDepthM;
    std::string stackDepthStatus;
    std::vector<double> laminationThicknessOptionsM;

    [[nodiscard]] bool hasMagneticGeometry() const noexcept;
    [[nodiscard]] bool hasCompleteCadGeometry() const noexcept;
};

/// Computes net limb area for a three-phase EI lamination from the imported
/// column width and explicitly supplied stack depth/stacking factor.
/// Returns NOT_SUPPORTED for component families without this outline model.
[[nodiscard]] Result<double> calculateThreePhaseEiEffectiveLimbAreaM2(
    const CoreComponentGeometrySnapshot& geometry,
    double stackDepthM,
    double stackingFactor);

//};

/// Decodes a pinned core-component revision. Missing geometry is preserved as
/// unavailable; malformed or non-positive supplied geometry is rejected.
[[nodiscard]] Result<CoreComponentGeometrySnapshot> loadCoreComponentGeometryRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId);

} // namespace p135::infrastructure::sqlite
