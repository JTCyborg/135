#include "p135/domain/initial_magnetization.hpp"

#include <cmath>
#include <utility>

namespace p135::domain {
namespace {
constexpr double oerstedToAmperePerMetre = 1000.0 / (4.0 * 3.14159265358979323846);
constexpr double gaussToTesla = 1.0e-4;

template <class T>
Result<T> fail(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "INITIAL_MAGNETIZATION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<T>::failure(std::move(error));
}

bool valid(const InitialMagnetizationEquation& e)
{
    return !e.sourceReference.empty() && !e.datasetRevision.empty() &&
        std::isfinite(e.initialPermeability) && e.initialPermeability > 0.0 &&
        std::isfinite(e.coefficientA) && e.coefficientA > 0.0 &&
        std::isfinite(e.exponentB) && e.exponentB > 1.0 &&
        std::isfinite(e.coefficientC) && e.coefficientC > 0.0 &&
        std::isfinite(e.exponentD) && e.exponentD > 0.0 &&
        std::isfinite(e.coefficientE) && e.coefficientE > 0.0 &&
        std::isfinite(e.minimumField_Oe) && e.minimumField_Oe == 0.0 &&
        std::isfinite(e.maximumField_Oe) && e.maximumField_Oe > 0.0;
}

double fluxDensityTesla(const InitialMagnetizationEquation& e, double hOe)
{
    if (hOe == 0.0) return 0.0;
    const double h = hOe;
    const double first = h + e.coefficientA * std::pow(h, e.exponentB);
    const double denominator = 1.0 / first +
        1.0 / (e.coefficientC * std::pow(h, e.exponentD)) +
        1.0 / e.coefficientE;
    return e.initialPermeability / denominator * gaussToTesla;
}
} // namespace

Result<units::FluxDensity> evaluateInitialMagnetization(
    const InitialMagnetizationEquation& equation,
    units::MagneticFieldStrength fieldStrength)
{
    if (!valid(equation))
        return fail<units::FluxDensity>(ErrorCode::InvalidInput,
            "Initial-magnetization coefficients, domain and provenance are invalid.",
            "equation");
    const double hApm = fieldStrength.si();
    if (!std::isfinite(hApm) || hApm < 0.0)
        return fail<units::FluxDensity>(ErrorCode::InvalidInput,
            "Magnetic field strength must be finite and non-negative.", "fieldStrength");
    const double hOe = hApm / oerstedToAmperePerMetre;
    if (hOe < equation.minimumField_Oe || hOe > equation.maximumField_Oe)
        return fail<units::FluxDensity>(ErrorCode::ModelOutOfDomain,
            "Field strength is outside the manufacturer's initial-magnetization equation range; no extrapolation was performed.",
            "fieldStrength");
    const double b = fluxDensityTesla(equation, hOe);
    if (!std::isfinite(b) || b < 0.0)
        return fail<units::FluxDensity>(ErrorCode::InvalidInput,
            "Initial-magnetization equation produced an invalid flux density.", "equation.result");
    return Result<units::FluxDensity>::success(units::FluxDensity{b});
}

Result<std::vector<BHPoint>> sampleInitialMagnetizationCurve(
    const InitialMagnetizationEquation& equation, std::size_t sampleCount)
{
    if (!valid(equation) || sampleCount < 2 || sampleCount > 100000)
        return fail<std::vector<BHPoint>>(ErrorCode::InvalidInput,
            "A valid equation and a sample count in [2, 100000] are required.",
            "sampleCount");
    std::vector<BHPoint> curve;
    curve.reserve(sampleCount);
    double previousB = -1.0;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const double hOe = equation.maximumField_Oe *
            static_cast<double>(i) / static_cast<double>(sampleCount - 1);
        const double hApm = hOe * oerstedToAmperePerMetre;
        const double b = fluxDensityTesla(equation, hOe);
        if (!std::isfinite(b) || b < 0.0 || (i > 0 && b <= previousB))
            return fail<std::vector<BHPoint>>(ErrorCode::InvalidInput,
                "Manufacturer equation did not produce a strictly increasing positive B-H branch.",
                "equation");
        curve.push_back(BHPoint{
            units::MagneticFieldStrength{hApm}, units::FluxDensity{b}
        });
        previousB = b;
    }
    return Result<std::vector<BHPoint>>::success(std::move(curve));
}

} // namespace p135::domain
