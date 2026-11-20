#pragma once

#include "p135/core/result.hpp"

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
    std::optional<std::uint32_t> parallelConductors;
    std::optional<double> conductorCrossSectionM2;
    std::optional<double> conductorWidthM;
    std::optional<double> conductorThicknessM;
    std::optional<double> turnInsulationBuildM;
    std::optional<double> layerInsulationBuildM;
    std::optional<double> mainInsulationBuildM;
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
[[nodiscard]] Result<void> validate(const MaterialRecommendationSet& recommendations);

/// True when a user has changed a material/physical input relative to the
/// last accepted design. The caller uses this to enable the unified recalculate action.
[[nodiscard]] bool requiresRecalculation(const ReactorDesignInputs& current,
                                         const ReactorDesignInputs& accepted) noexcept;

} // namespace p135::application
