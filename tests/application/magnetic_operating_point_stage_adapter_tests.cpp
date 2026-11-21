#include "p135/application/magnetic_operating_point_stage_adapter.hpp"

#include <cassert>
#include <string>

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
    g.limbCenterSpacings = {units::Length{0.10}, units::Length{0.10}, units::Length{0.20}};
    g.coreOverallHeight = units::Length{0.20};
    g.coreOverallWidth = units::Length{0.24};
    g.laminationConstruction = "measured laminated stack";
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
}

int main()
{
    PhysicalFilterReactorMagneticRequest request;
    request.physicalCore = validCore();
    request.stackingFactor = 0.95;
    request.dimensionalSource = "TEST-01 verified drawing";
    request.dimensionalRevision = "REV-A";
    request.filterMagnetic.filterBranchId = 7;
    request.filterMagnetic.magnetic.material.id = "core-revision-7";
    request.filterMagnetic.magnetic.material.sourceReference = "manufacturer B-H datasheet";
    request.filterMagnetic.magnetic.material.datasetRevision = "BH-REV-3";

    PhysicalFilterReactorMagneticResult result;
    result.magneticAnalysis.filterBranchId = 7;
    result.magneticAnalysis.excitations.push_back({
        1, {{{1.0, 0.0}, {-0.5, -0.8660254037844386}, {-0.5, 0.8660254037844386}}}
    });
    auto& cycle = result.magneticAnalysis.magneticCycle;
    cycle.converged = true;
    cycle.withinBhCurveRange = true;
    cycle.samplesUsed = 144;
    cycle.relativePeakChange = 0.0005;
    for (std::size_t i = 0; i < cycle.peakAbsoluteFluxDensity.size(); ++i)
        cycle.peakAbsoluteFluxDensity[i] = units::FluxDensity{0.7 + 0.01 * static_cast<double>(i)};

    const auto artifact = makeMagneticOperatingPointStageArtifact(request, result);
    assert(artifact);
    assert(artifact.value().find("schema=project135.magnetic-operating-point.v1") != std::string::npos);
    assert(artifact.value().find("dimensional_revision=REV-A") != std::string::npos);
    assert(artifact.value().find("bh_dataset_revision=BH-REV-3") != std::string::npos);
    assert(artifact.value().find("peak_absolute_flux_density_6_T=") != std::string::npos);

    auto zeroFluxBranch = result;
    zeroFluxBranch.magneticAnalysis.magneticCycle.peakAbsoluteFluxDensity[3] =
        units::FluxDensity{0.0};
    assert(makeMagneticOperatingPointStageArtifact(request, zeroFluxBranch));

    auto unconverged = result;
    unconverged.magneticAnalysis.magneticCycle.converged = false;
    assert(!makeMagneticOperatingPointStageArtifact(request, unconverged));

    auto extrapolated = result;
    extrapolated.magneticAnalysis.magneticCycle.withinBhCurveRange = false;
    assert(!makeMagneticOperatingPointStageArtifact(request, extrapolated));

    auto missingProvenance = request;
    missingProvenance.dimensionalRevision.clear();
    assert(!makeMagneticOperatingPointStageArtifact(missingProvenance, result));

    auto wrongBranch = result;
    wrongBranch.magneticAnalysis.filterBranchId = 8;
    assert(!makeMagneticOperatingPointStageArtifact(request, wrongBranch));
    return 0;
}
