#pragma once

#include "p135/core/result.hpp"

namespace p135::application {

/// Explicit assumptions for a first-pass idealized reactor sizing model.
/// SI units throughout. This does not select a catalog core or verify thermal design.
struct IdealReactorSizingRequest {
    double inductanceH{};
    double peakCurrentA{};
    double rmsCurrentA{};
    double allowablePeakFluxDensityT{};
    double allowableCurrentDensityApm2{};
    double windowUtilization{};
    double stackingFactor{};
    /// Chosen gross window area / gross core cross-section ratio Aw/Ae.
    double windowToCoreAreaRatio{};
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
    double assumedWindowToCoreAreaRatio{};
    double estimatedGrossCoreAreaM2{};
    double estimatedEffectiveCoreAreaM2{};
    double estimatedWindowAreaM2{};
    std::uint64_t idealMinimumTurns{};
    double idealTotalAirGapM{};
};

/// Computes an ideal area-product bound for a three-limb, three-phase reactor.
/// Each of the two core windows is assumed to carry two winding packs, hence
/// the factor 2 in Ae*Aw. Splits Ae/Aw using the explicit Aw/Ae ratio, then
/// estimates N >= L*Ipk/(Bmax*Aecore) and g = mu0*N^2*Aecore/L.
/// The gap estimate assumes all reluctance is in one ideal gap; it excludes
/// core reluctance, fringing, leakage, winding build, losses and standards checks.
[[nodiscard]] Result<IdealReactorSizingResult> calculateIdealReactorAreaProduct(
    const IdealReactorSizingRequest& request);

} // namespace p135::application
