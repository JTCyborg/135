#include "p135/application/physical_filter_reactor_magnetic.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <complex>

using namespace p135;
using namespace p135::application;

namespace {

domain::ThreeLimbCorePhysicalGeometry validCore()
{
    domain::ThreeLimbCorePhysicalGeometry g;
    g.limbWidths = {units::Length{0.04}, units::Length{0.04}, units::Length{0.04}};
    g.limbDepths = {units::Length{0.08}, units::Length{0.08}, units::Length{0.08}};
    g.windowWidths = {units::Length{0.06}, units::Length{0.06}};
    g.windowHeights = {units::Length{0.12}, units::Length{0.12}};
    g.yokeThicknesses.fill(units::Length{0.04});
    g.yokeDepths.fill(units::Length{0.08});
    g.limbCenterSpacings = {
        units::Length{0.10}, units::Length{0.10}, units::Length{0.20}
    };
    g.coreOverallHeight = units::Length{0.20};
    g.coreOverallWidth = units::Length{0.24};
    g.laminationConstruction = "unit-test measured construction";
    for (const auto interface : {
             domain::CoreJointInterface::TopYokeABToLimbA,
             domain::CoreJointInterface::TopYokeABToLimbB,
             domain::CoreJointInterface::TopYokeBCToLimbB,
             domain::CoreJointInterface::TopYokeBCToLimbC,
             domain::CoreJointInterface::BottomYokeABToLimbA,
             domain::CoreJointInterface::BottomYokeABToLimbB,
             domain::CoreJointInterface::BottomYokeBCToLimbB,
             domain::CoreJointInterface::BottomYokeBCToLimbC}) {
        g.joints.push_back({interface, domain::CoreJointType::Butt, {}});
    }
    return g;
}

PhysicalFilterReactorMagneticRequest makeRequest()
{
    PhysicalFilterReactorMagneticRequest request;
    request.physicalCore = validCore();
    request.stackingFactor = 0.95;
    auto& filterRequest = request.filterMagnetic;
    filterRequest.filterBranchId = 7;
    filterRequest.maximumSamples = 144;
    filterRequest.harmonicAnalysis.branchPoints.push_back(filter::BranchHarmonicPoint{
        7, 1, units::Frequency{50.0}, 1.0,
        std::array<std::complex<double>, 3>{{
            {1.0, 0.0}, {-0.5, -0.8660254037844386}, {-0.5, 0.8660254037844386}
        }}
    });
    filterRequest.filterBranchId = 7;
    filterRequest.magnetic.turns = {100, 100, 100};
    auto& material = filterRequest.magnetic.material;
    material.id = "physical-bridge-test-core";
    material.grade = "Test B-H curve";
    material.sourceReference = "deterministic unit-test fixture";
    material.datasetRevision = "test-rev-1";
    material.density = units::Density{7650.0};
    material.bhCurve = {
        {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
        {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.1}},
        {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.0}},
        {units::MagneticFieldStrength{5000.0}, units::FluxDensity{1.6}}
    };
    return request;
}

bool near(double a, double b, double tolerance = 1.0e-12)
{
    return std::abs(a - b) <= tolerance;
}

} // namespace

int main()
{
    const auto request = makeRequest();
    const auto result = analyzePhysicalFilterReactorMagneticCycle(request);
    assert(result);
    assert(result.value().magneticAnalysis.filterBranchId == 7);
    assert(result.value().magneticAnalysis.excitations.size() == 1);
    assert(result.value().magneticAnalysis.excitations.front().order == 1);
    assert(near(
        result.value().effectiveMagneticGeometry.limbEffectiveAreas[0]->si(),
        0.04 * 0.08 * 0.95));
    assert(result.value().magneticAnalysis.magneticCycle.samplesUsed >= 72);

    auto missingHarmonics = request;
    missingHarmonics.filterMagnetic.harmonicAnalysis.branchPoints.clear();
    assert(!analyzePhysicalFilterReactorMagneticCycle(missingHarmonics));

    auto invalidStacking = request;
    invalidStacking.stackingFactor = 0.0;
    assert(!analyzePhysicalFilterReactorMagneticCycle(invalidStacking));

    auto unsupportedGap = request;
    unsupportedGap.physicalCore.airGaps = {
        {domain::CoreSegment::YokeTopAB, units::Length{0.03}, units::Length{0.001}}
    };
    assert(!analyzePhysicalFilterReactorMagneticCycle(unsupportedGap));

    auto missingTurns = request;
    missingTurns.filterMagnetic.magnetic.turns[1] = 0;
    assert(!analyzePhysicalFilterReactorMagneticCycle(missingTurns));

    return 0;
}
