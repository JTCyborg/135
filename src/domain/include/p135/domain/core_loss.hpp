#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <string>
#include <vector>

namespace p135::domain {

/// One denominator term c / B^n in a manufacturer core-loss equation.
/// Coefficients use the source document's declared units.
struct CoreLossEquationTerm {
    double coefficient{};
    double fluxDensityExponent{};
};

/// Manufacturer equation of the form:
/// Pv = f / sum(c_i / B^n_i) + d * B^2 * f^2.
/// Axis scaling and output conversion are explicit because datasheets often
/// publish equations in non-SI units (for example gauss and mW/cm^3).
struct CoreLossEquation {
    std::vector<CoreLossEquationTerm> denominatorTerms;
    double eddyCoefficient{};
    double minimumFrequency_Hz{};
    double maximumFrequency_Hz{};
    double minimumFluxDensity_T{};
    double maximumFluxDensity_T{};
    double sourceFluxDensityUnitsPerTesla{};
    double outputWattsPerM3PerSourceUnit{};
    std::string sourceReference;
    std::string datasetRevision;
};

/// Evaluates only inside the published equation domain. No extrapolation.
[[nodiscard]] Result<units::CoreLossDensity> evaluateCoreLossEquation(
    const CoreLossEquation& equation,
    units::Frequency frequency,
    units::FluxDensity peakFluxDensity);

} // namespace p135::domain
