#include "p135/application/filter_reactor_magnetic.hpp"

#include <cassert>

using namespace p135;

int main()
{
    application::FilterReactorMagneticRequest request;
    request.filterBranchId = 7;

    auto missing = application::analyzeFilterReactorMagneticCycle(request);
    assert(!missing);

    request.harmonicAnalysis.branchPoints.push_back(filter::BranchHarmonicPoint{
        8, 1, units::Frequency{50.0}, 1.0,
        {{{1.0, 0.0}, {-0.5, -0.8660254037844386}, {-0.5, 0.8660254037844386}}}
    });
    auto wrongBranch = application::analyzeFilterReactorMagneticCycle(request);
    assert(!wrongBranch);

    request.harmonicAnalysis.branchPoints.push_back(filter::BranchHarmonicPoint{
        7, 1, units::Frequency{50.0}, 1.0,
        {{{1.0, 0.0}, {-0.5, -0.8660254037844386}, {-0.5, 0.8660254037844386}}}
    });
    request.harmonicAnalysis.branchPoints.push_back(filter::BranchHarmonicPoint{
        7, 5, units::Frequency{250.0}, 0.1,
        {{{0.1, 0.0}, {-0.05, 0.08660254037844386}, {-0.05, -0.08660254037844386}}}
    });
    request.magnetic.geometry.limbEffectiveArea = units::Area{1.0e-3};
    request.magnetic.geometry.limbMeanPath = units::Length{0.2};
    request.magnetic.geometry.limbAirGap = units::Length{5.0e-4};
    request.magnetic.geometry.yokeEffectiveArea = units::Area{1.0e-3};
    request.magnetic.geometry.yokeSegmentPath = units::Length{0.1};
    request.magnetic.turns = {100, 100, 100};
    request.magnetic.material.id = "test-core";
    request.magnetic.material.grade = "test-grade";
    request.magnetic.material.sourceReference = "unit-test";
    request.magnetic.material.datasetRevision = "1";
    request.magnetic.material.density = units::Density{7650.0};
    request.magnetic.material.bhCurve = {
        {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
        {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.1}},
        {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.0}}
    };
    request.maximumSamples = 144;

    const auto result = application::analyzeFilterReactorMagneticCycle(request);
    assert(result);
    assert(result.value().filterBranchId == 7);
    assert(result.value().excitations.size() == 2);
    assert(result.value().excitations[0].order == 1);
    assert(result.value().excitations[1].order == 5);
    assert(result.value().magneticCycle.samplesUsed >= 72);
    return 0;
}
