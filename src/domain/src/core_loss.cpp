#include "p135/domain/core_loss.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>
#include <tuple>
#include <utility>

namespace p135::domain {
namespace {
Result<units::VolumetricCoreLossDensity> fail(ErrorCode code, std::string message,
                                    std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CORE_LOSS_EQUATION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<units::VolumetricCoreLossDensity>::failure(std::move(error));
}
} // namespace

Result<units::VolumetricCoreLossDensity> evaluateCoreLossEquation(
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
    return Result<units::VolumetricCoreLossDensity>::success(
        units::VolumetricCoreLossDensity{lossWPerM3});
}

namespace {
Result<units::CoreLossDensity> failMeasuredData(
    ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CORE_LOSS_DATA_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<units::CoreLossDensity>::failure(std::move(error));
}

struct AxisBracket {
    double lower{};
    double upper{};
};

AxisBracket bracketAxis(const std::vector<double>& axis, double value)
{
    const auto upper = std::lower_bound(axis.begin(), axis.end(), value);
    if (upper == axis.end())
        return {axis.back(), axis.back()};
    if (*upper == value)
        return {value, value};
    if (upper == axis.begin())
        return {axis.front(), axis.front()};
    return {*std::prev(upper), *upper};
}

double interpolationWeight(const AxisBracket& bracket, double value)
{
    if (bracket.lower == bracket.upper)
        return 0.0;
    return (value - bracket.lower) / (bracket.upper - bracket.lower);
}
} // namespace

Result<units::CoreLossDensity> evaluateCoreLossData(
    const CoreMaterial& material,
    units::Frequency frequency,
    units::FluxDensity peakFluxDensity,
    units::TemperatureKelvin temperature)
{
    const double f = frequency.si();
    const double b = peakFluxDensity.si();
    const double t = temperature.si();
    if (!std::isfinite(f) || !std::isfinite(b) || !std::isfinite(t) ||
        f <= 0.0 || b <= 0.0 || t <= 0.0)
        return failMeasuredData(ErrorCode::InvalidInput,
            "Core-loss operating frequency, peak flux density and temperature must be finite and positive.",
            "operatingPoint");
    if (material.lossData.empty() || material.lossDataReference.empty() ||
        material.sourceReference.empty() || material.datasetRevision.empty())
        return failMeasuredData(ErrorCode::MissingRequiredData,
            "Measured core-loss data and source provenance are required.",
            "material.lossData");

    std::vector<double> frequencies;
    std::vector<double> fluxDensities;
    std::vector<double> temperatures;
    std::set<std::tuple<double, double, double>> coordinates;
    for (const auto& point : material.lossData) {
        const double pf = point.frequency.si();
        const double pb = point.peakFluxDensity.si();
        const double pt = point.temperature.si();
        const double loss = point.specificLoss.si();
        if (!std::isfinite(pf) || !std::isfinite(pb) || !std::isfinite(pt) ||
            !std::isfinite(loss) || pf <= 0.0 || pb <= 0.0 || pt <= 0.0 || loss < 0.0)
            return failMeasuredData(ErrorCode::InvalidInput,
                "Every measured core-loss point must contain finite positive coordinates and non-negative loss.",
                "material.lossData");
        if (!coordinates.emplace(pf, pb, pt).second)
            return failMeasuredData(ErrorCode::InvalidInput,
                "Measured core-loss data contains duplicate frequency/flux-density/temperature coordinates.",
                "material.lossData");
        frequencies.push_back(pf);
        fluxDensities.push_back(pb);
        temperatures.push_back(pt);
    }
    const auto uniqueSort = [](std::vector<double>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    uniqueSort(frequencies);
    uniqueSort(fluxDensities);
    uniqueSort(temperatures);

    const AxisBracket fb = bracketAxis(frequencies, f);
    const AxisBracket bb = bracketAxis(fluxDensities, b);
    const AxisBracket tb = bracketAxis(temperatures, t);
    if (f < frequencies.front() || f > frequencies.back() ||
        b < fluxDensities.front() || b > fluxDensities.back() ||
        t < temperatures.front() || t > temperatures.back())
        return failMeasuredData(ErrorCode::ModelOutOfDomain,
            "Core-loss operating point is outside the measured data domain; no extrapolation was performed.",
            "operatingPoint");

    const double wf = interpolationWeight(fb, f);
    const double wb = interpolationWeight(bb, b);
    const double wt = interpolationWeight(tb, t);
    double interpolated = 0.0;
    for (int fi = 0; fi < (fb.lower == fb.upper ? 1 : 2); ++fi) {
        const double cf = fi == 0 ? fb.lower : fb.upper;
        const double weightF = fb.lower == fb.upper ? 1.0 : (fi == 0 ? 1.0 - wf : wf);
        for (int bi = 0; bi < (bb.lower == bb.upper ? 1 : 2); ++bi) {
            const double cb = bi == 0 ? bb.lower : bb.upper;
            const double weightB = bb.lower == bb.upper ? 1.0 : (bi == 0 ? 1.0 - wb : wb);
            for (int ti = 0; ti < (tb.lower == tb.upper ? 1 : 2); ++ti) {
                const double ct = ti == 0 ? tb.lower : tb.upper;
                const double weightT = tb.lower == tb.upper ? 1.0 : (ti == 0 ? 1.0 - wt : wt);
                const auto found = std::find_if(material.lossData.begin(), material.lossData.end(),
                    [&](const CoreLossPoint& point) {
                        return point.frequency.si() == cf &&
                               point.peakFluxDensity.si() == cb &&
                               point.temperature.si() == ct;
                    });
                if (found == material.lossData.end())
                    return failMeasuredData(ErrorCode::MissingMaterialProperty,
                        "Measured core-loss grid is incomplete at a required interpolation corner.",
                        "material.lossData.grid");
                interpolated += weightF * weightB * weightT * found->specificLoss.si();
            }
        }
    }
    if (!std::isfinite(interpolated) || interpolated < 0.0)
        return failMeasuredData(ErrorCode::InvalidInput,
            "Interpolated core-loss density is not finite and non-negative.",
            "result");
    return Result<units::CoreLossDensity>::success(units::CoreLossDensity{interpolated});
}

} // namespace p135::domain
