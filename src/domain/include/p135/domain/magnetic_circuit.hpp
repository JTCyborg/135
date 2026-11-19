#pragma once

#include "p135/domain/magnetic_product.hpp"

#include <optional>

namespace p135::domain {

/// Nonlinear single-winding magnetic circuit using a measured B-H curve and
/// a lumped air gap. The model assumes uniform core flux and neglects fringing.
struct MagneticCircuitRequest {
    CoreGeometry core;
    CoreMaterial material;
    std::uint32_t turns{};
    units::Current current;
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
    units::Inductance secantInductance;
    units::Inductance differentialInductance;
    std::optional<units::FluxDensity> kneeFluxDensity;
    std::optional<units::Current> kneeCurrent;
    std::optional<double> kneeUtilization;
    bool withinBhCurveRange{false};
};

/// Solves N*I = H_core(B)*le + B*gap/mu0 by bounded bisection.
/// Returns ModelOutOfDomain instead of extrapolating beyond the measured B-H data.
[[nodiscard]] Result<MagneticCircuitResult>
analyzeMagneticCircuit(const MagneticCircuitRequest& request);

} // namespace p135::domain
