#pragma once

#include "p135/domain/magnetic_product.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace p135::domain {

/// Symmetric three-limb core reluctance network. The positive measured B-H
/// branch is mirrored for negative H; each limb has one winding and the two
/// yoke levels each contain two inter-limb segments.
struct ThreeLimbMagneticGeometry {
    units::Area limbEffectiveArea;
    units::Length limbMeanPath;
    units::Length limbAirGap;
    units::Area yokeEffectiveArea;
    units::Length yokeSegmentPath;
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

/// Solves the coupled six-node / seven-branch nonlinear reluctance network.
/// No B-H extrapolation is permitted. Negative excitation uses odd symmetry
/// of the supplied positive B-H branch; this assumption is explicit.
[[nodiscard]] Result<ThreeLimbMagneticResult>
analyzeThreeLimbMagneticCircuit(const ThreeLimbMagneticRequest& request);

} // namespace p135::domain
