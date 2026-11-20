#include "p135/domain/core_loss.hpp"

#include <cmath>
#include <utility>

namespace p135::domain {
namespace {
Result<units::CoreLossDensity> fail(ErrorCode code, std::string message,
                                    std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CORE_LOSS_EQUATION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<units::CoreLossDensity>::failure(std::move(error));
}
} // namespace

Result<units::CoreLossDensity> evaluateCoreLossEquation(
    const CoreLossEquation& equation,
    units::Frequency frequency,
    units::FluxDensity peakFluxDensity)
{
    const double f = frequency.si();
    const double bTesla = peakFluxDensity.si();
    if (!std::isfinite(f) || !std::isfinite(bTesla) || f <= 0.0 || bTesla <= 0.0)
        return fail(ErrorCode::InvalidInput,
                    "Frequency and peak flux density must be finite and positive.",
                    "operatingPoint");

    if (equation.sourceReference.empty() || equation.datasetRevision.empty() ||
        equation.denominatorTerms.empty())
        return fail(ErrorCode::MissingRequiredData,
                    "A core-loss equation requires source provenance, revision and denominator terms.",
                    "equation.provenance");

    if (!std::isfinite(equation.minimumFrequency_Hz) ||
        !std::isfinite(equation.maximumFrequency_Hz) ||
        equation.minimumFrequency_Hz <= 0.0 ||
        equation.maximumFrequency_Hz < equation.minimumFrequency_Hz ||
        !std::isfinite(equation.minimumFluxDensity_T) ||
        !std::isfinite(equation.maximumFluxDensity_T) ||
        equation.minimumFluxDensity_T <= 0.0 ||
        equation.maximumFluxDensity_T < equation.minimumFluxDensity_T ||
        !std::isfinite(equation.sourceFluxDensityUnitsPerTesla) ||
        equation.sourceFluxDensityUnitsPerTesla <= 0.0 ||
        !std::isfinite(equation.outputWattsPerM3PerSourceUnit) ||
        equation.outputWattsPerM3PerSourceUnit <= 0.0 ||
        !std::isfinite(equation.eddyCoefficient) || equation.eddyCoefficient < 0.0)
        return fail(ErrorCode::InvalidInput,
                    "Core-loss equation coefficients, unit conversions and domain are invalid.",
                    "equation");

    if (f < equation.minimumFrequency_Hz || f > equation.maximumFrequency_Hz ||
        bTesla < equation.minimumFluxDensity_T ||
        bTesla > equation.maximumFluxDensity_T)
        return fail(ErrorCode::ModelOutOfDomain,
                    "Requested frequency or flux density is outside the published core-loss equation domain; no extrapolation was performed.",
                    "operatingPoint");

    const double bSource = bTesla * equation.sourceFluxDensityUnitsPerTesla;
    double denominator = 0.0;
    for (const auto& term : equation.denominatorTerms) {
        if (!std::isfinite(term.coefficient) || term.coefficient <= 0.0 ||
            !std::isfinite(term.fluxDensityExponent) || term.fluxDensityExponent <= 0.0)
            return fail(ErrorCode::InvalidInput,
                        "Every denominator term requires positive finite coefficient and exponent.",
                        "equation.denominatorTerms");
        denominator += term.coefficient / std::pow(bSource, term.fluxDensityExponent);
    }
    if (!std::isfinite(denominator) || denominator <= 0.0)
        return fail(ErrorCode::InvalidInput,
                    "Core-loss equation denominator is not finite and positive.",
                    "equation.denominatorTerms");

    const double sourceLoss = f / denominator +
        equation.eddyCoefficient * bSource * bSource * f * f;
    const double lossWPerM3 =
        sourceLoss * equation.outputWattsPerM3PerSourceUnit;
    if (!std::isfinite(lossWPerM3) || lossWPerM3 < 0.0)
        return fail(ErrorCode::InvalidInput,
                    "Core-loss equation produced a non-finite or negative result.",
                    "equation.result");
    return Result<units::CoreLossDensity>::success(
        units::CoreLossDensity{lossWPerM3});
}

} // namespace p135::domain
