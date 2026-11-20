#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/magnetic_product.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace p135::domain {

/// Manufacturer initial-magnetization equation. Source axes are Oe and gauss;
/// the public API accepts SI H and returns SI B.
struct InitialMagnetizationEquation {
    double initialPermeability{};
    double coefficientA{};
    double exponentB{};
    double coefficientC{};
    double exponentD{};
    double coefficientE{};
    double minimumField_Oe{};
    double maximumField_Oe{};
    std::string sourceReference;
    std::string datasetRevision;
};

/// Evaluate B(H) = mu_i / [1/(H+a*H^b) + 1/(c*H^d) + 1/e].
/// The result is bounded by the manufacturer's published H interval.
[[nodiscard]] Result<units::FluxDensity> evaluateInitialMagnetization(
    const InitialMagnetizationEquation& equation,
    units::MagneticFieldStrength fieldStrength);

/// Samples the equation into the B-H representation consumed by the existing
/// nonlinear magnetic-circuit solvers. Samples are model-generated, not
/// measured points; the requested solver must still be kept inside this range.
[[nodiscard]] Result<std::vector<BHPoint>> sampleInitialMagnetizationCurve(
    const InitialMagnetizationEquation& equation, std::size_t sampleCount);

} // namespace p135::domain
