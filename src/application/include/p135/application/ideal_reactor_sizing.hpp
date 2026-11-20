#pragma once

#include "p135/core/result.hpp"

namespace p135::application {

/// Explicit assumptions for a first-pass, idealized reactor area-product bound.
/// All quantities use SI units. This is not a geometry or thermal solver.
struct IdealReactorSizingRequest {
    double inductanceH{};
    double peakCurrentA{};
    double rmsCurrentA{};
    double allowablePeakFluxDensityT{};
    double allowableCurrentDensityApm2{};
    double windowUtilization{};
    double stackingFactor{};
};

struct IdealReactorSizingResult {
    double minimumEffectiveAreaProductM4{};
    double minimumGrossAreaProductM4{};
    double equivalentSquareSideM{};
    double storedPeakEnergyJ{};
    double assumedAllowablePeakFluxDensityT{};
    double assumedAllowableCurrentDensityApm2{};
    double assumedWindowUtilization{};
    double assumedStackingFactor{};
};

/// Computes Ae*Aw >= L*Ipeak*Irms/(Bpeak*J*Ku), corrected for stacking factor.
/// The result is only a lower bound: it does not select a core, turns, gap,
/// winding layout, losses, insulation, temperature rise, or standards compliance.
[[nodiscard]] Result<IdealReactorSizingResult> calculateIdealReactorAreaProduct(
    const IdealReactorSizingRequest& request);

} // namespace p135::application
