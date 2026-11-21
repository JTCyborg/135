#include "p135/application/reactor_design_inputs.hpp"

#include <cmath>
#include <array>
#include <algorithm>
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

    if (inputs.coreYokeThicknessM &&
        (!std::isfinite(*inputs.coreYokeThicknessM) || *inputs.coreYokeThicknessM <= 0.0))
        return invalid("Core yoke thickness must be finite and greater than zero.");

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
    if (!validText(inputs.coreGeometryFamily) || !validText(inputs.coreLaminationConstruction) ||
        !validText(inputs.dimensionalSource) || !validText(inputs.dimensionalRevision) ||
        !validText(inputs.conductorShape) || !validText(inputs.windingTechnology) ||
        !validText(inputs.coolingMethod) || !validText(inputs.bobbinMaterialId))
        return invalid("Specified construction identifiers must not be empty.");
    if (inputs.dimensionalSource.has_value() != inputs.dimensionalRevision.has_value())
        return invalid("Dimensional source and revision must be supplied together.");

    const auto validCount = [](const auto& value) { return !value || *value > 0; };
    if (!validCount(inputs.turnsPerPhase) || !validCount(inputs.turnsPerLayer) ||
        !validCount(inputs.windingLayers) || !validCount(inputs.windingSections) ||
        !validCount(inputs.parallelConductors))
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
    const auto validArray = [](const auto& values) {
        return !values || std::all_of(values->begin(), values->end(),
            [](double value) { return std::isfinite(value) && value > 0.0; });
    };
    const auto validNonNegativeArray = [](const auto& values) {
        return !values || std::all_of(values->begin(), values->end(),
            [](double value) { return std::isfinite(value) && value >= 0.0; });
    };
    if (!validArray(inputs.limbWidthsM) || !validArray(inputs.limbStackDepthsM) ||
        !validArray(inputs.windowWidthsM) || !validArray(inputs.windowHeightsM) ||
        !validArray(inputs.yokeThicknessesM) || !validArray(inputs.yokeDepthsM) ||
        !validArray(inputs.limbCenterSpacingsM) || !validArray(inputs.bobbinInnerWidthsM) ||
        !validArray(inputs.bobbinInnerHeightsM) || !validArray(inputs.bobbinAxialLengthsM) ||
        !validArray(inputs.bobbinWallThicknessesM) || !validArray(inputs.bobbinFlangeThicknessesM) ||
        !validArray(inputs.bobbinFlangeOuterWidthsM) || !validArray(inputs.bobbinFlangeOuterHeightsM) ||
        !validNonNegativeArray(inputs.bobbinWindingClearancesM) ||
        !validArray(inputs.windingRadialBuildsM) ||
        !validArray(inputs.windingAxialHeightsM) || !validArray(inputs.windingEndClearancesM))
        return invalid("Specified physical core, bobbin and winding dimensions must be finite and greater than zero.");
    if (inputs.coreOverallHeightM &&
        (!std::isfinite(*inputs.coreOverallHeightM) || *inputs.coreOverallHeightM <= 0.0))
        return invalid("Core overall height must be finite and greater than zero.");
    if (inputs.coreOverallWidthM &&
        (!std::isfinite(*inputs.coreOverallWidthM) || *inputs.coreOverallWidthM <= 0.0))
        return invalid("Core overall width must be finite and greater than zero.");
    if (inputs.coreJointConstructions) {
        for (std::size_t joint = 0; joint < inputs.coreJointConstructions->size(); ++joint) {
            const auto& construction = (*inputs.coreJointConstructions)[joint];
            if (construction != "butt" && construction != "step-lap")
                return invalid("Each core joint must explicitly select a supported construction type.");
            const auto& packets = inputs.coreJointStepLapPackets[joint];
            if (construction == "butt" && !packets.empty())
                return invalid("Butt joints cannot contain Step-Lap packet records.");
            if (construction == "step-lap" && packets.empty())
                return invalid("Every Step-Lap joint requires an explicit packet schedule.");
            for (const auto& packet : packets) {
                if (!std::isfinite(packet.stackDepthOffsetM) || packet.stackDepthOffsetM < 0.0 ||
                    !std::isfinite(packet.stackDepthM) || packet.stackDepthM <= 0.0 ||
                    !std::isfinite(packet.overlapIntoLimbM) || packet.overlapIntoLimbM <= 0.0 ||
                    !std::isfinite(packet.overlapIntoYokeM) || packet.overlapIntoYokeM <= 0.0 ||
                    (packet.direction != "limb-into-yoke" && packet.direction != "yoke-into-limb"))
                    return invalid("Each Step-Lap packet requires finite positive geometry and an explicit alternating direction.");
            }
        }
    } else {
        for (const auto& packets : inputs.coreJointStepLapPackets)
            if (!packets.empty())
                return invalid("Step-Lap packet schedules require all eight joint construction selectors.");
    }
    if (inputs.airGapCountPerLimb && inputs.totalAirGapLengthPerLimbM) {
        for (std::size_t i = 0; i < 3; ++i) {
            const auto count = (*inputs.airGapCountPerLimb)[i];
            const auto total = (*inputs.totalAirGapLengthPerLimbM)[i];
            if (!std::isfinite(total) || total < 0.0 || ((count == 0) != (total == 0.0)))
                return invalid("Air-gap count and total length must agree per limb; zero gaps require zero total length.");
        }
    }
    for (const auto& gap : inputs.airGaps) {
        if (gap.limbIndex >= 3 || !std::isfinite(gap.positionFromLimbStartM) ||
            !std::isfinite(gap.lengthM) || gap.positionFromLimbStartM < 0.0 || gap.lengthM <= 0.0)
            return invalid("Each discrete air gap requires a valid limb, non-negative position and positive length.");
    }
    if (inputs.airGapCountPerLimb) {
        std::array<std::uint32_t, 3> actual{};
        for (const auto& gap : inputs.airGaps)
            if (gap.limbIndex < 3) ++actual[gap.limbIndex];
        if (!inputs.airGaps.empty() && actual != *inputs.airGapCountPerLimb)
            return invalid("Discrete air-gap records must match the declared count on each limb.");
    }
    if (inputs.radialCoolingDuctCount && *inputs.radialCoolingDuctCount != inputs.coolingDucts.size())
        return invalid("Cooling duct count must match the explicitly defined duct records.");
    for (const auto& duct : inputs.coolingDucts) {
        if (!std::isfinite(duct.positionFromWindingBottomM) ||
            !std::isfinite(duct.thicknessM) || duct.positionFromWindingBottomM < 0.0 ||
            duct.thicknessM <= 0.0 || duct.spacerMaterialRevisionId.empty() ||
            duct.airflowDirection.empty())
            return invalid("Each cooling duct requires position, positive thickness, spacer material revision and airflow direction.");
    }
    if (!validPositive(inputs.conductorBareDiameterM) ||
        !validPositive(inputs.conductorInsulatedDiameterM) ||
        !validPositive(inputs.conductorInsulationBuildM))
        return invalid("Round conductor diameters and insulation build must be finite and greater than zero.");
    if (inputs.conductorShape && *inputs.conductorShape == "round" &&
        inputs.conductorBareDiameterM && inputs.conductorInsulatedDiameterM &&
        *inputs.conductorInsulatedDiameterM < *inputs.conductorBareDiameterM)
        return invalid("Insulated round-conductor diameter cannot be smaller than its bare diameter.");
    if (inputs.insulationTemperatureReference) {
        const auto& t = *inputs.insulationTemperatureReference;
        if (t.thermalClass.empty() || t.standardReference.empty() ||
            !std::isfinite(t.referenceTemperatureK) || t.referenceTemperatureK <= 0.0 ||
            !std::isfinite(t.safetyMarginK) || t.safetyMarginK < 0.0 ||
            !std::isfinite(t.maximumDesignWindingTemperatureK) ||
            t.maximumDesignWindingTemperatureK <= 0.0 ||
            t.maximumDesignWindingTemperatureK > t.referenceTemperatureK - t.safetyMarginK)
            return invalid("Insulation thermal class reference and maximum design winding temperature are inconsistent or incomplete.");
    }
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
           current.coreYokeThicknessM != accepted.coreYokeThicknessM ||
           current.ambientTemperatureK != accepted.ambientTemperatureK ||
           current.maximumWindingTemperatureK != accepted.maximumWindingTemperatureK ||
           current.coreGeometryFamily != accepted.coreGeometryFamily ||
           current.coreLaminationConstruction != accepted.coreLaminationConstruction ||
           current.conductorShape != accepted.conductorShape ||
           current.windingTechnology != accepted.windingTechnology ||
           current.coolingMethod != accepted.coolingMethod ||
           current.bobbinUsed != accepted.bobbinUsed ||
           current.bobbinMaterialId != accepted.bobbinMaterialId ||
           current.turnsPerPhase != accepted.turnsPerPhase ||
           current.turnsPerLayer != accepted.turnsPerLayer ||
           current.windingLayers != accepted.windingLayers ||
           current.windingSections != accepted.windingSections ||
           current.parallelConductors != accepted.parallelConductors ||
           current.conductorCrossSectionM2 != accepted.conductorCrossSectionM2 ||
           current.conductorWidthM != accepted.conductorWidthM ||
           current.conductorThicknessM != accepted.conductorThicknessM ||
           current.turnInsulationBuildM != accepted.turnInsulationBuildM ||
           current.layerInsulationBuildM != accepted.layerInsulationBuildM ||
           current.mainInsulationBuildM != accepted.mainInsulationBuildM ||
           current.limbWidthsM != accepted.limbWidthsM ||
           current.limbStackDepthsM != accepted.limbStackDepthsM ||
           current.windowWidthsM != accepted.windowWidthsM ||
           current.windowHeightsM != accepted.windowHeightsM ||
           current.yokeThicknessesM != accepted.yokeThicknessesM ||
           current.yokeDepthsM != accepted.yokeDepthsM ||
           current.limbCenterSpacingsM != accepted.limbCenterSpacingsM ||
           current.coreOverallHeightM != accepted.coreOverallHeightM ||
           current.coreOverallWidthM != accepted.coreOverallWidthM ||
           current.coreJointConstructions != accepted.coreJointConstructions ||
           current.coreJointStepLapPackets != accepted.coreJointStepLapPackets ||
           current.airGapCountPerLimb != accepted.airGapCountPerLimb ||
           current.totalAirGapLengthPerLimbM != accepted.totalAirGapLengthPerLimbM ||
           current.airGaps != accepted.airGaps ||
           current.bobbinInnerWidthsM != accepted.bobbinInnerWidthsM ||
           current.bobbinInnerHeightsM != accepted.bobbinInnerHeightsM ||
           current.bobbinAxialLengthsM != accepted.bobbinAxialLengthsM ||
           current.bobbinWallThicknessesM != accepted.bobbinWallThicknessesM ||
           current.bobbinFlangeThicknessesM != accepted.bobbinFlangeThicknessesM ||
           current.bobbinFlangeOuterWidthsM != accepted.bobbinFlangeOuterWidthsM ||
           current.bobbinFlangeOuterHeightsM != accepted.bobbinFlangeOuterHeightsM ||
           current.windingRadialBuildsM != accepted.windingRadialBuildsM ||
           current.windingAxialHeightsM != accepted.windingAxialHeightsM ||
           current.windingEndClearancesM != accepted.windingEndClearancesM ||
           current.radialCoolingDuctCount != accepted.radialCoolingDuctCount ||
           current.coolingDucts != accepted.coolingDucts ||
           current.conductorBareDiameterM != accepted.conductorBareDiameterM ||
           current.conductorInsulatedDiameterM != accepted.conductorInsulatedDiameterM ||
           current.conductorInsulationBuildM != accepted.conductorInsulationBuildM ||
           current.insulationTemperatureReference != accepted.insulationTemperatureReference;
}

} // namespace p135::application
