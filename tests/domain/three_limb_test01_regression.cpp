#include "p135/domain/three_limb_magnetic.hpp"

#include <array>
#include <cmath>
#include <complex>
#include <iostream>
#include <numbers>

using namespace p135;
using namespace p135::domain;

namespace {
std::complex<double> phasor(double rms, double degrees)
{
    return std::polar(rms, degrees * std::numbers::pi / 180.0);
}

ThreeLimbMagneticCycleSamplingRequest testScenario()
{
    ThreeLimbMagneticCycleSamplingRequest request;
    request.magnetic.geometry.limbEffectiveArea = units::Area{4200.0e-6};
    request.magnetic.geometry.limbMeanPath = units::Length{680.0e-3};
    request.magnetic.geometry.limbAirGap = units::Length{8.5e-3};
    request.magnetic.geometry.yokeEffectiveArea = units::Area{4200.0e-6};
    request.magnetic.geometry.yokeSegmentPath = units::Length{220.0e-3};
    request.magnetic.geometry.limbGapEffectiveAreas.fill(units::Area{4800.0e-6});
    request.magnetic.turns = {26, 26, 26};

    auto& material = request.magnetic.material;
    material.id = "TEST-01-M4-3408";
    material.grade = "M4 / 3408 approximate user-supplied curve";
    material.sourceReference = "Project135 calculation-history-all.p135history / TEST-01";
    material.datasetRevision = "2026-11-20-user-supplied";
    material.bhCurve = {
        {units::MagneticFieldStrength{0.0},    units::FluxDensity{0.0}},
        {units::MagneticFieldStrength{50.0},   units::FluxDensity{0.8}},
        {units::MagneticFieldStrength{100.0},  units::FluxDensity{1.2}},
        {units::MagneticFieldStrength{250.0},  units::FluxDensity{1.45}},
        {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.65}},
        {units::MagneticFieldStrength{5000.0}, units::FluxDensity{1.82}}
    };

    // TEST-01: 5th negative-sequence harmonic, 80 A RMS at 180 degrees;
    // 7th positive-sequence harmonic, 45 A RMS at 0 degrees.
    request.harmonicExcitations = {
        {5, {phasor(80.0, 180.0), phasor(80.0, 300.0), phasor(80.0, 60.0)}},
        {7, {phasor(45.0, 0.0), phasor(45.0, -120.0), phasor(45.0, 120.0)}}
    };
    request.phaseDcCurrents = {
        units::Current{0.0}, units::Current{0.0}, units::Current{0.0}
    };
    request.relativePeakTolerance = 1.0e-3;
    request.maximumSamples = 576;
    return request;
}
} // namespace

int main()
{
    const auto request = testScenario();
    const auto result = sampleThreeLimbMagneticCycle(request);
    if (!result) {
        std::cerr << "TEST-01 three-limb magnetic regression FAILED: "
                  << result.error().message << '\n';
        return 1;
    }

    std::cout << "TEST-01 three-limb magnetic regression: solver completed; "
              << "samples=" << result.value().samplesUsed
              << ", converged=" << (result.value().converged ? "yes" : "no")
              << ", within_BH=" << (result.value().withinBhCurveRange ? "yes" : "no")
              << ", peak_change=" << result.value().relativePeakChange << '\n';
    for (std::size_t i = 0; i < result.value().peakAbsoluteFluxDensity.size(); ++i) {
        std::cout << "  branch[" << i << "] peak_B="
                  << result.value().peakAbsoluteFluxDensity[i].si() << " T\n";
    }

    if (!result.value().converged) {
        std::cerr << "TEST-01 magnetic cycle did not meet the 1e-3 peak convergence "
                     "criterion within 576 samples.\n";
        return 2;
    }
    return 0;
}
