#pragma once

#include "p135/domain/three_limb_magnetic.hpp"
#include "p135/filter/harmonics.hpp"

#include <cstdint>
#include <vector>

namespace p135::application {

struct FilterReactorMagneticRequest {
    std::uint32_t filterBranchId{};
    filter::HarmonicAnalysisResult harmonicAnalysis;
    domain::ThreeLimbMagneticRequest magnetic;
    double relativePeakTolerance{1.0e-3};
    std::size_t maximumSamples{576};
};

struct FilterReactorMagneticResult {
    std::uint32_t filterBranchId{};
    std::vector<domain::ThreeLimbMagneticHarmonicExcitation> excitations;
    domain::ThreeLimbMagneticCycleSamplingResult magneticCycle;
};

/// Transfers the selected filter branch's per-phase harmonic winding currents
/// into the nonlinear three-limb magnetic cycle solver. Geometry, turns and
/// material must be supplied explicitly; this function performs no sizing.
[[nodiscard]] Result<FilterReactorMagneticResult>
analyzeFilterReactorMagneticCycle(const FilterReactorMagneticRequest& request);

} // namespace p135::application
