#pragma once

#include "p135/core/result.hpp"
#include "p135/domain/mechanical_geometry.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace p135::application {

enum class DesignInputOrigin { Recommended, UserSelected };
enum class RecommendationStatus { Preliminary, Verified, NotVerified };

struct MaterialRevisionSelection {
    std::string revisionId;
    DesignInputOrigin origin{DesignInputOrigin::Recommended};
    bool operator==(const MaterialRevisionSelection&) const = default;
};

// Physical construction inputs are optional until supplied by a drawing,
// datasheet or measurement. All lengths are SI metres; no defaults are inferred.
struct CoreAirGapInput {
    std::uint32_t limbIndex{}; // A=0, B=1, C=2
    double positionFromLimbStartM{};
    double lengthM{};
    bool operator==(const CoreAirGapInput&) const = default;
};

struct CoolingDuctInput {
    double positionFromWindingBottomM{};
    double thicknessM{};
    std::string spacerMaterialRevisionId;
    std::string airflowDirection; // e.g. axial, radial, transverse; explicit source required
    bool operator==(const CoolingDuctInput&) const = default;
};

struct InsulationTemperatureReference {
    std::string thermalClass; // e.g. F, H; reference only, not a design hotspot limit
    std::string standardReference;
    double referenceTemperatureK{};
    double safetyMarginK{};
    double maximumDesignWindingTemperatureK{};
    DesignInputOrigin origin{DesignInputOrigin::Recommended};
    bool operator==(const InsulationTemperatureReference&) const = default;
};

/// Explicit inputs for the physical reactor-design stage.
/// The filter snapshot is the immutable electrical-analysis source; material
/// revisions are pinned, never silently replaced by newer catalog revisions.
struct ReactorDesignInputs {
    std::string sourceFilterSnapshotId;
    std::uint32_t filterBranchId{};
    std::optional<MaterialRevisionSelection> coreMaterial;
    std::optional<MaterialRevisionSelection> conductorMaterial;
    std::optional<MaterialRevisionSelection> insulationMaterial;
    std::optional<double> stackingFactor;
    /// User-selected laminated-core stack depth; never inferred from EI outline size.
    std::optional<double> coreStackDepthM;
    /// Explicit physical yoke thickness; not inferred from ambiguous catalogue dimensions.
    std::optional<double> coreYokeThicknessM;
    std::optional<double> ambientTemperatureK;
    std::optional<double> maximumWindingTemperatureK;

    // Construction choices and dimensions; all dimensional values use SI units.
    std::optional<std::string> coreGeometryFamily;
    std::optional<std::string> conductorShape;
    std::optional<std::string> windingTechnology;
    std::optional<std::string> coolingMethod;
    std::optional<bool> bobbinUsed;
    std::optional<std::string> bobbinMaterialId;
    std::optional<std::uint32_t> turnsPerPhase;
    std::optional<std::uint32_t> turnsPerLayer;
    std::optional<std::uint32_t> windingLayers;
    std::optional<std::uint32_t> windingSections;
    std::optional<std::uint32_t> parallelConductors;
    std::optional<double> conductorCrossSectionM2;
    std::optional<double> conductorWidthM;
    std::optional<double> conductorThicknessM;
    std::optional<double> turnInsulationBuildM;
    std::optional<double> layerInsulationBuildM;
    std::optional<double> mainInsulationBuildM;
    // Explicit three-limb physical core dimensions (A/B/C, left/right windows).
    std::optional<std::array<double, 3>> limbWidthsM;
    std::optional<std::array<double, 3>> limbStackDepthsM;
    std::optional<std::array<double, 2>> windowWidthsM;
    std::optional<std::array<double, 2>> windowHeightsM;
    std::optional<std::array<double, 4>> yokeThicknessesM;
    std::optional<std::array<double, 4>> yokeDepthsM;
    std::optional<std::array<double, 3>> limbCenterSpacingsM; // AB, BC, AC
    std::optional<double> coreOverallHeightM;
    std::optional<double> coreOverallWidthM;
    // Explicit construction for the eight limb-to-yoke interfaces in domain enum order.
    std::optional<std::array<std::string, 8>> coreJointConstructions;
    std::optional<std::string> coreLaminationConstruction;
    // Provenance for the physical dimensions used to construct CAD; both are required together.
    std::optional<std::string> dimensionalSource;
    std::optional<std::string> dimensionalRevision;

    // Gap count and total gap length are distinct from individual gap positions.
    std::optional<std::array<std::uint32_t, 3>> airGapCountPerLimb;
    std::optional<std::array<double, 3>> totalAirGapLengthPerLimbM;
    std::vector<CoreAirGapInput> airGaps;

    // Bobbin and winding-pack dimensions, one entry per phase/limb.
    std::optional<std::array<double, 3>> bobbinInnerWidthsM;
    std::optional<std::array<double, 3>> bobbinInnerHeightsM;
    std::optional<std::array<double, 3>> bobbinAxialLengthsM;
    std::optional<std::array<double, 3>> bobbinWallThicknessesM;
    std::optional<std::array<double, 3>> bobbinFlangeThicknessesM;
    std::optional<std::array<double, 3>> bobbinFlangeOuterWidthsM;
    std::optional<std::array<double, 3>> bobbinFlangeOuterHeightsM;
    std::optional<std::array<double, 3>> bobbinWindingClearancesM;
    std::optional<std::array<double, 3>> windingRadialBuildsM;
    std::optional<std::array<double, 3>> windingAxialHeightsM;
    std::optional<std::array<double, 3>> windingEndClearancesM;

    // Cooling ducts are explicit geometric features, not an area correction.
    std::optional<std::uint32_t> radialCoolingDuctCount;
    std::vector<CoolingDuctInput> coolingDucts;

    // Cross-section applicability: round uses bare/insulated diameter;
    // rectangular/foil use bare width/thickness and insulation build.
    std::optional<double> conductorBareDiameterM;
    std::optional<double> conductorInsulatedDiameterM;
    std::optional<double> conductorInsulationBuildM;

    std::optional<InsulationTemperatureReference> insulationTemperatureReference;
};

/// A candidate is evidence, not a promise that a physical design has passed.
/// objectiveMetrics are named, unit-qualified values calculated by real models.
struct MaterialRecommendation {
    std::string materialRevisionId;
    std::string materialRole;
    std::vector<std::string> evidenceReferences;
    std::vector<std::string> unmetRequirements;
};

struct MaterialRecommendationSet {
    std::string sourceFilterSnapshotId;
    std::uint32_t filterBranchId{};
    RecommendationStatus status{RecommendationStatus::Preliminary};
    std::vector<std::string> objectiveIds;
    std::vector<MaterialRecommendation> candidates;
};

[[nodiscard]] Result<void> validate(const ReactorDesignInputs& inputs);
// Converts only explicit, complete physical dimensions. Missing inputs are errors;
// electrical effective Ae/Aw/le values are never used as CAD dimensions.
[[nodiscard]] Result<domain::ReactorPhysicalGeometry> makeReactorPhysicalGeometry(
    const ReactorDesignInputs& inputs, const std::string& productId);
[[nodiscard]] Result<void> validate(const MaterialRecommendationSet& recommendations);

/// True when a user has changed a material/physical input relative to the
/// last accepted design. The caller uses this to enable the unified recalculate action.
[[nodiscard]] bool requiresRecalculation(const ReactorDesignInputs& current,
                                         const ReactorDesignInputs& accepted) noexcept;

} // namespace p135::application
