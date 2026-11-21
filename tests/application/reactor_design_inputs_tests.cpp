#include "p135/application/reactor_design_inputs.hpp"

#include <cassert>
#include <array>
#include <limits>

using namespace p135::application;

int main()
{
    ReactorDesignInputs inputs;
    assert(!validate(inputs));

    inputs.sourceFilterSnapshotId = "filter-snapshot-42";
    inputs.filterBranchId = 1;
    assert(validate(inputs));

    inputs.coreMaterial = MaterialRevisionSelection{"core-rev-3", DesignInputOrigin::Recommended};
    inputs.conductorMaterial = MaterialRevisionSelection{"cu-rev-2", DesignInputOrigin::Recommended};
    inputs.insulationMaterial = MaterialRevisionSelection{"ins-rev-5", DesignInputOrigin::Recommended};
    inputs.stackingFactor = 0.95;
    inputs.coreStackDepthM = 0.12;
    inputs.coreYokeThicknessM = 0.03;
    inputs.ambientTemperatureK = 298.15;
    inputs.maximumWindingTemperatureK = 393.15;
    inputs.coreGeometryFamily = "three-limb";
    inputs.coreLaminationConstruction = "measured packet stack";
    inputs.dimensionalSource = "Core drawing DR-125";
    inputs.dimensionalRevision = "Rev C";
    inputs.conductorShape = "rectangular";
    inputs.windingTechnology = "layer";
    inputs.coolingMethod = "natural-air";
    inputs.bobbinUsed = false;
    inputs.turnsPerPhase = 120;
    inputs.turnsPerLayer = 12;
    inputs.windingLayers = 10;
    inputs.windingSections = 3;
    inputs.parallelConductors = 2;
    inputs.conductorCrossSectionM2 = 2.5e-6;
    inputs.conductorWidthM = 2.0e-3;
    inputs.conductorThicknessM = 1.25e-3;
    inputs.turnInsulationBuildM = 0.1e-3;
    inputs.layerInsulationBuildM = 0.2e-3;
    inputs.mainInsulationBuildM = 1.0e-3;
    assert(validate(inputs));

    const auto accepted = inputs;
    assert(!requiresRecalculation(inputs, accepted));

    inputs.dimensionalRevision = "Rev D";
    assert(requiresRecalculation(inputs, accepted));
    inputs.dimensionalRevision = "Rev C";
    inputs.coreStackDepthM = 0.125;
    assert(requiresRecalculation(inputs, accepted));
    inputs.coreStackDepthM = 0.12;
    inputs.dimensionalRevision.reset();
    assert(!validate(inputs));
    inputs.dimensionalRevision = "Rev C";
    inputs.coreYokeThicknessM = 0.031;
    assert(requiresRecalculation(inputs, accepted));
    inputs.coreYokeThicknessM = 0.03;

    inputs.turnsPerPhase = 121;
    assert(requiresRecalculation(inputs, accepted));
    inputs.turnsPerPhase = 120;
    inputs.mainInsulationBuildM = 1.1e-3;
    assert(requiresRecalculation(inputs, accepted));
    inputs.mainInsulationBuildM = 1.0e-3;

    inputs.conductorMaterial = MaterialRevisionSelection{"cu-user-rev-8", DesignInputOrigin::UserSelected};
    assert(requiresRecalculation(inputs, accepted));
    assert(validate(inputs));

    inputs.coreStackDepthM = 0.0;
    assert(!validate(inputs));
    inputs.coreStackDepthM = 0.12;
    inputs.coreYokeThicknessM = 0.0;
    assert(!validate(inputs));
    inputs.coreYokeThicknessM = 0.03;
    inputs.stackingFactor = 1.2;
    assert(!validate(inputs));
    inputs.stackingFactor = 0.95;
    inputs.maximumWindingTemperatureK = 290.0;
    assert(!validate(inputs));
    inputs.maximumWindingTemperatureK = 393.15;
    inputs.turnsPerLayer = 0;
    assert(!validate(inputs));
    inputs.turnsPerLayer = 12;
    inputs.conductorWidthM = std::numeric_limits<double>::quiet_NaN();
    assert(!validate(inputs));

    // Explicit physical construction dimensions participate in validation
    // and invalidate any previously accepted revision.
    inputs.limbWidthsM = std::array<double, 3>{0.04, 0.05, 0.04};
    inputs.limbStackDepthsM = std::array<double, 3>{0.12, 0.12, 0.12};
    inputs.windowWidthsM = std::array<double, 2>{0.10, 0.10};
    inputs.windowHeightsM = std::array<double, 2>{0.20, 0.20};
    inputs.yokeThicknessesM = std::array<double, 4>{0.04, 0.04, 0.04, 0.04};
    inputs.yokeDepthsM = std::array<double, 4>{0.12, 0.12, 0.12, 0.12};
    inputs.limbCenterSpacingsM = std::array<double, 3>{0.15, 0.15, 0.30};
    inputs.coreOverallHeightM = 0.28;
    inputs.coreOverallWidthM = 0.40;
    inputs.airGapCountPerLimb = std::array<std::uint32_t, 3>{1, 0, 1};
    inputs.totalAirGapLengthPerLimbM = std::array<double, 3>{0.001, 0.0, 0.001};
    inputs.airGaps = {{0, 0.08, 0.001}, {2, 0.08, 0.001}};
    inputs.bobbinInnerWidthsM = std::array<double, 3>{0.05, 0.06, 0.05};
    inputs.bobbinInnerHeightsM = std::array<double, 3>{0.13, 0.13, 0.13};
    inputs.bobbinAxialLengthsM = std::array<double, 3>{0.18, 0.18, 0.18};
    inputs.bobbinWallThicknessesM = std::array<double, 3>{0.001, 0.001, 0.001};
    inputs.bobbinFlangeThicknessesM = std::array<double, 3>{0.003, 0.003, 0.003};
    inputs.bobbinFlangeOuterWidthsM = std::array<double, 3>{0.056, 0.066, 0.056};
    inputs.bobbinFlangeOuterHeightsM = std::array<double, 3>{0.136, 0.136, 0.136};
    inputs.windingRadialBuildsM = std::array<double, 3>{0.02, 0.02, 0.02};
    inputs.windingAxialHeightsM = std::array<double, 3>{0.16, 0.16, 0.16};
    inputs.windingEndClearancesM = std::array<double, 3>{0.01, 0.01, 0.01};
    inputs.radialCoolingDuctCount = 1;
    inputs.coolingDucts = {{0.08, 0.002, "spacer-rev-1", "radial"}};
    inputs.conductorBareDiameterM.reset();
    inputs.conductorInsulatedDiameterM.reset();
    inputs.conductorInsulationBuildM.reset();
    inputs.insulationTemperatureReference = InsulationTemperatureReference{
        "F", "IEC 60085", 428.15, 10.0, 418.15, DesignInputOrigin::UserSelected};
    assert(validate(inputs));
    const auto physicalAccepted = inputs;
    inputs.windowWidthsM = std::array<double, 2>{0.0, 0.10};
    assert(!validate(inputs));
    inputs = physicalAccepted;
    inputs.airGaps.pop_back();
    assert(!validate(inputs));
    inputs = physicalAccepted;
    inputs.coolingDucts[0].thicknessM = 0.0;
    assert(!validate(inputs));
    inputs = physicalAccepted;
    inputs.insulationTemperatureReference->maximumDesignWindingTemperatureK = 430.0;
    assert(!validate(inputs));
    inputs = physicalAccepted;
    inputs.totalAirGapLengthPerLimbM = std::array<double, 3>{0.002, 0.0, 0.001};
    assert(requiresRecalculation(inputs, physicalAccepted));
    inputs = physicalAccepted;
    inputs.coreLaminationConstruction = "different packet stack";
    assert(requiresRecalculation(inputs, physicalAccepted));
    inputs = physicalAccepted;
    inputs.coreJointConstructions = std::array<std::string, 8>{
        "butt", "butt", "butt", "butt", "butt", "butt", "butt", "butt"};
    assert(requiresRecalculation(inputs, physicalAccepted));
    inputs = physicalAccepted;
    inputs.windingSections = 4;
    assert(requiresRecalculation(inputs, physicalAccepted));
    inputs = physicalAccepted;
    inputs.bobbinFlangeThicknessesM = std::array<double, 3>{0.004, 0.003, 0.003};
    assert(requiresRecalculation(inputs, physicalAccepted));
    inputs = physicalAccepted;
    inputs.coreJointConstructions = std::array<std::string, 8>{
        "butt", "butt", "invalid", "butt", "butt", "butt", "butt", "butt"};
    assert(!validate(inputs));

    MaterialRecommendationSet recommendations;
    recommendations.sourceFilterSnapshotId = "filter-snapshot-42";
    recommendations.filterBranchId = 1;
    recommendations.status = RecommendationStatus::Preliminary;
    recommendations.candidates.push_back({"core-rev-3", "core", {"BH:core-rev-3"}, {}});
    assert(validate(recommendations));
    recommendations.candidates.push_back({"core-rev-3", "core", {}, {}});
    assert(!validate(recommendations));
    return 0;
}
