#pragma once

#include "p135/domain/magnetic_product.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <complex>
#include <optional>

namespace p135::domain {

/// Coupled three-limb core reluctance network. Each limb and yoke segment may
/// have independent geometry. The positive measured B-H branch is mirrored
/// for negative H; each limb has one winding and the two yoke levels each
/// contain two inter-limb segments.
struct ThreeLimbMagneticGeometry {
    // Legacy symmetric defaults. Per-branch values below override these
    // when set, preserving existing callers and saved inputs.
    units::Area limbEffectiveArea;
    units::Length limbMeanPath;
    units::Length limbAirGap;
    units::Area yokeEffectiveArea;
    units::Length yokeSegmentPath;

    // Optional independent geometry: limbs A/B/C; yokes top AB, top BC,
    // bottom AB, bottom BC. An unset entry inherits its legacy scalar value; explicit zero is validated normally.
    std::array<std::optional<units::Area>, 3> limbEffectiveAreas{};
    std::array<std::optional<units::Length>, 3> limbMeanPaths{};
    std::array<std::optional<units::Length>, 3> limbAirGaps{};
    /// Effective gap areas A/B/C; unset entries use each limb core area (no fringing).
    std::array<std::optional<units::Area>, 3> limbGapEffectiveAreas{};
    std::array<std::optional<units::Area>, 4> yokeEffectiveAreas{};
    std::array<std::optional<units::Length>, 4> yokeSegmentPaths{};
};

struct ThreeLimbMagneticRequest {
    ThreeLimbMagneticGeometry geometry;
    std::array<std::uint32_t, 3> turns{};
    std::array<units::Current, 3> phaseCurrents{};
    CoreMaterial material;
    std::size_t maximumIterations{80};
    double relativeFluxTolerance{1.0e-9};
};

struct ThreeLimbMagneticResult {
    // Signed values preserve phase polarity. Indices A, B, C.
    std::array<units::FluxDensity, 3> limbFluxDensity{};
    std::array<units::MagneticFlux, 3> limbFlux{};
    // Segment order: top A-B, top B-C, bottom A-B, bottom B-C.
    std::array<units::FluxDensity, 4> yokeFluxDensity{};
    std::array<units::MagneticFlux, 4> yokeFlux{};
    std::array<units::MagneticPotential, 6> nodeMagneticPotential{};
    std::size_t iterations{};
    double maximumFluxResidual_Wb{};
    bool withinBhCurveRange{false};
};

struct ThreeLimbMagneticCycleSamplingRequest {
    ThreeLimbMagneticRequest magnetic;
    std::array<std::complex<double>, 3> phaseRmsPhasors{};
    // Per-phase DC current bias, added to the instantaneous AC waveform.
    std::array<units::Current, 3> phaseDcCurrents{};
    double relativePeakTolerance{1.0e-3};
    std::size_t maximumSamples{288}; // supported caps: 144 or 288; first pass is 72
};

struct ThreeLimbMagneticCycleSamplingResult {
    // Absolute peak B in A/B/C limbs followed by top AB, top BC,
    // bottom AB and bottom BC yoke segments.
    std::array<units::FluxDensity, 7> peakAbsoluteFluxDensity{};
    std::size_t samplesUsed{};
    double relativePeakChange{};
    bool converged{false};
};

/// Samples one electrical cycle on nested 72/144/288 grids. Convergence is
/// based on the maximum relative change across all seven branch peak |B| values.
/// Reaching maximumSamples without meeting tolerance returns a successful
/// diagnostic result with converged=false (NOT_VERIFIED), not a solver failure.
[[nodiscard]] Result<ThreeLimbMagneticCycleSamplingResult>
sampleThreeLimbMagneticCycle(const ThreeLimbMagneticCycleSamplingRequest& request);

/// Solves the coupled six-node / seven-branch nonlinear reluctance network.
/// No B-H extrapolation is permitted. Negative excitation uses odd symmetry
/// of the supplied positive B-H branch; this assumption is explicit.
[[nodiscard]] Result<ThreeLimbMagneticResult>
analyzeThreeLimbMagneticCircuit(const ThreeLimbMagneticRequest& request);

} // namespace p135::domain
