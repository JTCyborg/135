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
    std::optional<double> ambientTemperatureK;
    std::optional<double> maximumWindingTemperatureK;
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
