#include "p135/domain/three_limb_magnetic.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

using namespace p135;
using namespace p135::domain;

namespace {

ThreeLimbMagneticRequest makeRequest()
{
    ThreeLimbMagneticRequest request;
    request.geometry.limbEffectiveArea = units::Area{4200.0e-6};
    request.geometry.limbMeanPath = units::Length{680.0e-3};
    request.geometry.limbAirGap = units::Length{8.5e-3};
    request.geometry.yokeEffectiveArea = units::Area{4200.0e-6};
    request.geometry.yokeSegmentPath = units::Length{220.0e-3};
    request.geometry.limbGapEffectiveAreas.fill(units::Area{4800.0e-6});
    request.turns = {26, 26, 26};

    request.material.id = "THREE-LIMB-UNIT-TEST";
    request.material.grade = "Test B-H curve";
    request.material.sourceReference = "Deterministic unit-test fixture";
    request.material.datasetRevision = "1";
    request.material.bhCurve = {
        {units::MagneticFieldStrength{0.0},    units::FluxDensity{0.0}},
        {units::MagneticFieldStrength{50.0},   units::FluxDensity{0.8}},
        {units::MagneticFieldStrength{100.0},  units::FluxDensity{1.2}},
        {units::MagneticFieldStrength{250.0},  units::FluxDensity{1.45}},
        {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.65}},
        {units::MagneticFieldStrength{5000.0}, units::FluxDensity{1.82}}
    };
    return request;
}

bool near(double left, double right, double tolerance)
{
    return std::abs(left - right) <= tolerance;
}

} // namespace

int main()
{
    // Zero excitation must be a numerically converged equilibrium and must not
    // spuriously report B-H extrapolation.
    {
        const auto result = analyzeThreeLimbMagneticCircuit(makeRequest());
        assert(result);
        assert(result.value().iterations == 0);
        assert(near(result.value().maximumFluxResidual_Wb, 0.0, 1.0e-12));
        assert(result.value().withinBhCurveRange);
        for (const auto value : result.value().limbFluxDensity)
            assert(value.si() == 0.0);
        for (const auto value : result.value().yokeFluxDensity)
            assert(value.si() == 0.0);
    }

    // The implemented positive B-H branch is mirrored for negative excitation.
    // Reversing all phase currents must reverse signed branch flux densities
    // without changing their magnitudes.
    {
        auto positiveRequest = makeRequest();
        positiveRequest.phaseCurrents = {
            units::Current{80.0}, units::Current{-35.0}, units::Current{-45.0}
        };
        auto negativeRequest = positiveRequest;
        for (std::size_t phase = 0; phase < 3; ++phase)
            negativeRequest.phaseCurrents[phase] =
                units::Current{-positiveRequest.phaseCurrents[phase].si()};

        const auto positive = analyzeThreeLimbMagneticCircuit(positiveRequest);
        const auto negative = analyzeThreeLimbMagneticCircuit(negativeRequest);
        assert(positive);
        assert(negative);
        for (std::size_t i = 0; i < 3; ++i) {
            assert(near(positive.value().limbFluxDensity[i].si(),
                        -negative.value().limbFluxDensity[i].si(), 1.0e-8));
        }
        for (std::size_t i = 0; i < 4; ++i) {
            assert(near(positive.value().yokeFluxDensity[i].si(),
                        -negative.value().yokeFluxDensity[i].si(), 1.0e-8));
        }
    }

    // Invalid iteration controls must be rejected rather than silently
    // returning an unconverged magnetic state.
    {
        auto request = makeRequest();
        request.maximumIterations = 0;
        const auto result = analyzeThreeLimbMagneticCircuit(request);
        assert(!result);
        assert(result.error().code == ErrorCode::InvalidInput);
    }

    // A one-iteration budget with a deliberately strong, asymmetric excitation
    // must not be reported as a successful converged solution.
    {
        auto request = makeRequest();
        request.phaseCurrents = {
            units::Current{1800.0}, units::Current{-250.0}, units::Current{-900.0}
        };
        request.maximumIterations = 1;
        const auto result = analyzeThreeLimbMagneticCircuit(request);
        assert(!result);
        assert(result.error().code == ErrorCode::NonConvergence ||
               result.error().code == ErrorCode::SingularSystem);
    }

    return 0;
}
