#include "p135/application/magnetic_operating_point_stage_adapter.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <string>

using namespace p135;
using namespace p135::application;

namespace {
bool hasNumericValue(const std::string& artifact, const std::string& key, double expected)
{
    const auto position = artifact.find(key + "=");
    if (position == std::string::npos)
        return false;
    const auto valueStart = position + key.size() + 1;
    const auto valueEnd = artifact.find('\n', valueStart);
    try {
        const double actual = std::stod(artifact.substr(valueStart, valueEnd - valueStart));
        return std::abs(actual - expected) <= 1.0e-12 * std::max({1.0, std::abs(actual), std::abs(expected)});
    } catch (...) {
        return false;
    }
}

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
    // Synthetic fixture evidence exercises the positive path only; it is not
    // a claim that any imported catalogue material has a verified release.
    request.bhCurveSourceStatus = "MEASURED_TABULAR";
    request.bhCurveModelStatus = "SOURCE_CURVE_PRESENT";
    request.designReleaseStatus = "VERIFIED";
    request.filterMagnetic.filterBranchId = 7;
    request.filterMagnetic.harmonicAnalysis.branchPoints.push_back(filter::BranchHarmonicPoint{
        7, 1, units::Frequency{50.0}, 1.0,
        std::array<std::complex<double>, 3>{{{1.0, 0.0}, {-0.5, -0.8660254037844386},
                                             {-0.5, 0.8660254037844386}}}
    });
    request.filterMagnetic.magnetic.material.id = "core-revision-7";
    request.filterMagnetic.magnetic.material.manufacturer = "Test Manufacturer";
    request.filterMagnetic.magnetic.material.grade = "Test Electrical Steel";
    request.filterMagnetic.magnetic.material.sourceReference = "manufacturer B-H datasheet";
    request.filterMagnetic.magnetic.material.datasetRevision = "BH-REV-3";
    request.filterMagnetic.magnetic.turns = {120, 120, 120};
    request.filterMagnetic.magnetic.material.bhCurve = {
        {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
        {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.1}},
        {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.0}}
    };

    PhysicalFilterReactorMagneticResult result;
    const auto effectiveGeometry = domain::buildThreeLimbMagneticGeometry(request.physicalCore, request.stackingFactor);
    assert(effectiveGeometry);
    result.effectiveMagneticGeometry = effectiveGeometry.value();
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
    assert(artifact.value().find("physical_limb_width_0_m=0.04") != std::string::npos);
    assert(hasNumericValue(artifact.value(), "physical_core_overall_width_m", 0.24));
    assert(artifact.value().find("turns_phase_0=120") != std::string::npos);
    assert(artifact.value().find("bh_curve.2.B_T=1") != std::string::npos);
    assert(artifact.value().find("excitation.0.frequency_Hz=50") != std::string::npos);
    assert(artifact.value().find("magnetic_geometry.limb_area_m2=") != std::string::npos);

    auto zeroFluxBranch = result;
    zeroFluxBranch.magneticAnalysis.magneticCycle.peakAbsoluteFluxDensity[3] =
        units::FluxDensity{0.0};
    assert(makeMagneticOperatingPointStageArtifact(request, zeroFluxBranch));

    assert(artifact.value().find("bh_curve_source_status=MEASURED_TABULAR") != std::string::npos);
    assert(artifact.value().find("bh_curve_model_status=SOURCE_CURVE_PRESENT") != std::string::npos);
    assert(artifact.value().find("design_release_status=VERIFIED") != std::string::npos);

    auto estimatedMaterialEvidence = request;
    estimatedMaterialEvidence.bhCurveSourceStatus = "NOT_PUBLISHED_IN_IMPORTED_DATASET";
    estimatedMaterialEvidence.bhCurveModelStatus = "ESTIMATED_FROM_SPARSE_MAGNETIC_ANCHORS";
    estimatedMaterialEvidence.designReleaseStatus = "NOT_VERIFIED";
    assert(!makeMagneticOperatingPointStageArtifact(estimatedMaterialEvidence, result));

    auto digitizedMaterialEvidence = request;
    digitizedMaterialEvidence.bhCurveSourceStatus = "APPROXIMATE_DIGITIZATION";
    digitizedMaterialEvidence.designReleaseStatus = "NOT_VERIFIED";
    assert(!makeMagneticOperatingPointStageArtifact(digitizedMaterialEvidence, result));

    auto missingTurns = request;
    missingTurns.filterMagnetic.magnetic.turns[1] = 0;
    assert(!makeMagneticOperatingPointStageArtifact(missingTurns, result));

    auto missingBhCurve = request;
    missingBhCurve.filterMagnetic.magnetic.material.bhCurve.clear();
    assert(!makeMagneticOperatingPointStageArtifact(missingBhCurve, result));

    auto mismatchedGeometry = result;
    mismatchedGeometry.effectiveMagneticGeometry.limbEffectiveArea = units::Area{0.123};
    assert(!makeMagneticOperatingPointStageArtifact(request, mismatchedGeometry));

    auto mismatchedExcitation = result;
    mismatchedExcitation.magneticAnalysis.excitations.front().phaseRmsPhasors[0] = {9.0, 0.0};
    assert(!makeMagneticOperatingPointStageArtifact(request, mismatchedExcitation));

    auto unconverged = result;
    unconverged.magneticAnalysis.magneticCycle.converged = false;
    assert(!makeMagneticOperatingPointStageArtifact(request, unconverged));

    auto extrapolated = result;
    extrapolated.magneticAnalysis.magneticCycle.withinBhCurveRange = false;
    assert(!makeMagneticOperatingPointStageArtifact(request, extrapolated));

    auto injectedProvenance = request;
    injectedProvenance.filterMagnetic.magnetic.material.grade = "steel=forged";
    assert(!makeMagneticOperatingPointStageArtifact(injectedProvenance, result));

    injectedProvenance = request;
    injectedProvenance.dimensionalSource = "drawing\nforged_key=value";
    assert(!makeMagneticOperatingPointStageArtifact(injectedProvenance, result));

    auto missingProvenance = request;
    missingProvenance.dimensionalRevision.clear();
    assert(!makeMagneticOperatingPointStageArtifact(missingProvenance, result));

    auto wrongBranch = result;
    wrongBranch.magneticAnalysis.filterBranchId = 8;
    assert(!makeMagneticOperatingPointStageArtifact(request, wrongBranch));
    return 0;
}
