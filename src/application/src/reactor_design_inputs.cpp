#include "p135/application/reactor_design_inputs.hpp"

#include <cmath>
#include <unordered_set>

namespace p135::application {
namespace {
Result<void> invalid(const char* message)
{
    return Result<void>::failure({ErrorCode::InvalidInput, message, {}});
}

bool sameSelection(const std::optional<MaterialRevisionSelection>& a,
                   const std::optional<MaterialRevisionSelection>& b) noexcept
{
    if (a.has_value() != b.has_value()) return false;
    if (!a) return true;
    return a->revisionId == b->revisionId && a->origin == b->origin;
}
}

Result<void> validate(const ReactorDesignInputs& inputs)
{
    if (inputs.sourceFilterSnapshotId.empty())
        return invalid("A source filter calculation snapshot is required.");
    if (inputs.filterBranchId == 0)
        return invalid("A non-zero filter branch ID is required.");

    const auto validSelection = [](const auto& selection) {
        return !selection || !selection->revisionId.empty();
    };
    if (!validSelection(inputs.coreMaterial) ||
        !validSelection(inputs.conductorMaterial) ||
        !validSelection(inputs.insulationMaterial))
        return invalid("Selected material revision IDs must not be empty.");

    if (inputs.coreStackDepthM &&
        (!std::isfinite(*inputs.coreStackDepthM) || *inputs.coreStackDepthM <= 0.0))
        return invalid("Core stack depth must be finite and greater than zero.");

    if (inputs.stackingFactor &&
        (!std::isfinite(*inputs.stackingFactor) ||
         *inputs.stackingFactor <= 0.0 || *inputs.stackingFactor > 1.0))
        return invalid("Stacking factor must be finite and in the interval (0, 1].");

    if (inputs.ambientTemperatureK &&
        (!std::isfinite(*inputs.ambientTemperatureK) || *inputs.ambientTemperatureK <= 0.0))
        return invalid("Ambient temperature must be a finite absolute temperature in kelvin.");

    if (inputs.maximumWindingTemperatureK &&
        (!std::isfinite(*inputs.maximumWindingTemperatureK) ||
         *inputs.maximumWindingTemperatureK <= 0.0))
        return invalid("Maximum winding temperature must be a finite absolute temperature in kelvin.");

    if (inputs.ambientTemperatureK && inputs.maximumWindingTemperatureK &&
        *inputs.maximumWindingTemperatureK <= *inputs.ambientTemperatureK)
        return invalid("Maximum winding temperature must exceed ambient temperature.");

    const auto validText = [](const auto& value) {
        return !value || !value->empty();
    };
    if (!validText(inputs.coreGeometryFamily) || !validText(inputs.conductorShape) ||
        !validText(inputs.windingTechnology) || !validText(inputs.coolingMethod) ||
        !validText(inputs.bobbinMaterialId))
        return invalid("Specified construction identifiers must not be empty.");

    const auto validCount = [](const auto& value) { return !value || *value > 0; };
    if (!validCount(inputs.turnsPerPhase) || !validCount(inputs.turnsPerLayer) ||
        !validCount(inputs.windingLayers) || !validCount(inputs.parallelConductors))
        return invalid("Specified winding counts must be greater than zero.");

    const auto validPositive = [](const auto& value) {
        return !value || (std::isfinite(*value) && *value > 0.0);
    };
    if (!validPositive(inputs.conductorCrossSectionM2) ||
        !validPositive(inputs.conductorWidthM) ||
        !validPositive(inputs.conductorThicknessM) ||
        !validPositive(inputs.turnInsulationBuildM) ||
        !validPositive(inputs.layerInsulationBuildM) ||
        !validPositive(inputs.mainInsulationBuildM))
        return invalid("Specified conductor and insulation dimensions must be finite and greater than zero.");
    return Result<void>::success();
}

Result<void> validate(const MaterialRecommendationSet& recommendations)
{
    if (recommendations.sourceFilterSnapshotId.empty())
        return invalid("Recommendations must reference their source filter snapshot.");
    if (recommendations.filterBranchId == 0)
        return invalid("Recommendations must reference a non-zero filter branch ID.");

    std::unordered_set<std::string> candidateKeys;
    for (const auto& candidate : recommendations.candidates) {
        if (candidate.materialRevisionId.empty() || candidate.materialRole.empty())
            return invalid("Each recommendation requires a material revision ID and role.");
        const auto key = candidate.materialRole + "\n" + candidate.materialRevisionId;
        if (!candidateKeys.insert(key).second)
            return invalid("Duplicate material recommendation for the same role and revision.");
    }
    for (const auto& objective : recommendations.objectiveIds) {
        if (objective.empty())
            return invalid("Recommendation objective IDs must not be empty.");
    }
    return Result<void>::success();
}

bool requiresRecalculation(const ReactorDesignInputs& current,
                           const ReactorDesignInputs& accepted) noexcept
{
    return current.sourceFilterSnapshotId != accepted.sourceFilterSnapshotId ||
           current.filterBranchId != accepted.filterBranchId ||
           !sameSelection(current.coreMaterial, accepted.coreMaterial) ||
           !sameSelection(current.conductorMaterial, accepted.conductorMaterial) ||
           !sameSelection(current.insulationMaterial, accepted.insulationMaterial) ||
           current.stackingFactor != accepted.stackingFactor ||
           current.coreStackDepthM != accepted.coreStackDepthM ||
           current.ambientTemperatureK != accepted.ambientTemperatureK ||
           current.maximumWindingTemperatureK != accepted.maximumWindingTemperatureK ||
           current.coreGeometryFamily != accepted.coreGeometryFamily ||
           current.conductorShape != accepted.conductorShape ||
           current.windingTechnology != accepted.windingTechnology ||
           current.coolingMethod != accepted.coolingMethod ||
           current.bobbinUsed != accepted.bobbinUsed ||
           current.bobbinMaterialId != accepted.bobbinMaterialId ||
           current.turnsPerPhase != accepted.turnsPerPhase ||
           current.turnsPerLayer != accepted.turnsPerLayer ||
           current.windingLayers != accepted.windingLayers ||
           current.parallelConductors != accepted.parallelConductors ||
           current.conductorCrossSectionM2 != accepted.conductorCrossSectionM2 ||
           current.conductorWidthM != accepted.conductorWidthM ||
           current.conductorThicknessM != accepted.conductorThicknessM ||
           current.turnInsulationBuildM != accepted.turnInsulationBuildM ||
           current.layerInsulationBuildM != accepted.layerInsulationBuildM ||
           current.mainInsulationBuildM != accepted.mainInsulationBuildM;
}

} // namespace p135::application
