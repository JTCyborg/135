#include "p135/application/ideal_reactor_sizing.hpp"

#include <cassert>
#include <cmath>
#include <limits>

using namespace p135::application;

int main()
{
    const IdealReactorSizingRequest request{
        0.01, 100.0, 70.0, 1.2, 3.0e6, 0.35, 0.95, 2.0
    };
    const auto result = calculateIdealReactorAreaProduct(request);
    assert(result);
    // Two winding packs occupy each window in the three-limb three-phase topology.
    const double expected = 2.0 * 0.01 * 100.0 * 70.0 / (1.2 * 3.0e6 * 0.35);
    assert(std::abs(result.value().minimumEffectiveAreaProductM4 - expected) < expected * 1.0e-12);
    assert(std::abs(result.value().minimumGrossAreaProductM4 - expected / 0.95) < expected * 1.0e-12);
    assert(std::abs(result.value().storedPeakEnergyJ - 50.0) < 1.0e-12);
    assert(std::abs(result.value().equivalentSquareSideM * result.value().equivalentSquareSideM -
        result.value().minimumGrossAreaProductM4) < 1.0e-18);
    assert(std::abs(result.value().estimatedWindowAreaM2 /
        result.value().estimatedGrossCoreAreaM2 - 2.0) < 1.0e-12);
    assert(std::abs(result.value().estimatedEffectiveCoreAreaM2 /
        result.value().estimatedGrossCoreAreaM2 - 0.95) < 1.0e-12);
    assert(result.value().idealMinimumTurns >= 1);
    const double requiredFlux = 0.01 * 100.0 /
        (static_cast<double>(result.value().idealMinimumTurns) *
         result.value().estimatedEffectiveCoreAreaM2);
    assert(requiredFlux <= 1.2 * (1.0 + 1.0e-12));
    constexpr double mu0 = 1.25663706212e-6;
    const double expectedGap = mu0 * result.value().idealMinimumTurns *
        result.value().idealMinimumTurns * result.value().estimatedEffectiveCoreAreaM2 / 0.01;
    assert(std::abs(result.value().idealTotalAirGapM - expectedGap) < expectedGap * 1.0e-12);

    auto bad = request;
    bad.allowablePeakFluxDensityT = 0.0;
    assert(!calculateIdealReactorAreaProduct(bad));
    bad = request;
    bad.windowUtilization = 1.01;
    assert(!calculateIdealReactorAreaProduct(bad));
    bad = request;
    bad.stackingFactor = std::numeric_limits<double>::quiet_NaN();
    assert(!calculateIdealReactorAreaProduct(bad));
    bad = request;
    bad.rmsCurrentA = -1.0;
    assert(!calculateIdealReactorAreaProduct(bad));
    bad = request;
    bad.windowToCoreAreaRatio = 0.0;
    assert(!calculateIdealReactorAreaProduct(bad));
    return 0;
}
