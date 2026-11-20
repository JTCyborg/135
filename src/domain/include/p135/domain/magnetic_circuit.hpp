#pragma once

#include "p135/domain/magnetic_product.hpp"

#include <optional>

namespace p135::domain {

/// Nonlinear single-winding magnetic circuit using a measured B-H curve and
/// a lumped air gap. Fringing requires an explicitly supplied effective gap area.
struct MagneticCircuitRequest {
    CoreGeometry core;
    CoreMaterial material;
    std::uint32_t turns{};
    units::Current current;
    /// Explicit effective pole-face area; absent means no fringing correction (Ag,eff=Ae).
    std::optional<units::Area> gapEffectiveArea;
    // Knee is the first B-H segment whose differential slope falls to this
    // fraction of the initial segment slope. This is a declared criterion,
    // not a universal material saturation constant.
    double kneeSlopeRatio{0.2};
};

struct MagneticCircuitResult {
    units::Current current;
    units::FluxDensity fluxDensity;
    units::MagneticFieldStrength coreFieldStrength;
    units::MagneticFieldStrength gapFieldStrength;
    units::MagneticFlux flux;
    units::Area coreAreaAtGap;
    units::Area effectiveGapArea;
    double gapFringingFactor{1.0};
    units::Inductance secantInductance;
    units::Inductance differentialInductance;
    std::optional<units::FluxDensity> kneeFluxDensity;
    std::optional<units::Current> kneeCurrent;
    std::optional<double> kneeUtilization;
    bool withinBhCurveRange{false};
};

/// Solves N*I = H_core(B)*le + B*Ae*gap/(mu0*Ag,eff) by bounded bisection.
/// Returns ModelOutOfDomain instead of extrapolating beyond the measured B-H data.
[[nodiscard]] Result<MagneticCircuitResult>
analyzeMagneticCircuit(const MagneticCircuitRequest& request);

} // namespace p135::domain
