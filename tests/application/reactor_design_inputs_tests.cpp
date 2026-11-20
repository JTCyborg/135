#include "p135/application/reactor_design_inputs.hpp"

#include <cassert>

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
    inputs.ambientTemperatureK = 298.15;
    inputs.maximumWindingTemperatureK = 393.15;
    assert(validate(inputs));

    const auto accepted = inputs;
    assert(!requiresRecalculation(inputs, accepted));

    inputs.conductorMaterial = MaterialRevisionSelection{"cu-user-rev-8", DesignInputOrigin::UserSelected};
    assert(requiresRecalculation(inputs, accepted));
    assert(validate(inputs));

    inputs.stackingFactor = 1.2;
    assert(!validate(inputs));
    inputs.stackingFactor = 0.95;
    inputs.maximumWindingTemperatureK = 290.0;
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
