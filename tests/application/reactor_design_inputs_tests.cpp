#include "p135/application/reactor_design_inputs.hpp"

#include <cassert>
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
    inputs.ambientTemperatureK = 298.15;
    inputs.maximumWindingTemperatureK = 393.15;
    inputs.coreGeometryFamily = "three-limb";
    inputs.conductorShape = "rectangular";
    inputs.windingTechnology = "layer";
    inputs.coolingMethod = "natural-air";
    inputs.bobbinUsed = false;
    inputs.turnsPerPhase = 120;
    inputs.turnsPerLayer = 12;
    inputs.windingLayers = 10;
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

    inputs.coreStackDepthM = 0.125;
    assert(requiresRecalculation(inputs, accepted));
    inputs.coreStackDepthM = 0.12;

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
