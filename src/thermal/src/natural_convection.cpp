#include "p135/thermal/natural_convection.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace p135::thermal {
namespace {

Error invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "NATURAL_CONVECTION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

Result<NaturalConvectionResult> analyzeVerticalPlateNaturalConvection(
    const VerticalPlateNaturalConvectionRequest& request)
{
    const double area = request.exposedArea.si();
    const double length = request.verticalCharacteristicLength.si();
    const double ambient = request.ambientTemperature.si();
    const double surface = request.surfaceTemperature.si();
    const auto& fluid = request.fluid;

    if (!finitePositive(area))
        return Result<NaturalConvectionResult>::failure(
            invalid("Exposed plate area must be finite and positive.", "exposedArea"));
    if (!finitePositive(length))
        return Result<NaturalConvectionResult>::failure(
            invalid("Vertical characteristic length must be finite and positive.", "verticalCharacteristicLength"));
    if (!finitePositive(ambient) || !finitePositive(surface) || surface <= ambient)
        return Result<NaturalConvectionResult>::failure(
            invalid("Surface temperature must be finite and above ambient for this heating correlation.",
                    "surfaceTemperature"));
    if (!finitePositive(fluid.thermalConductivityWPerMK) ||
        !finitePositive(fluid.kinematicViscosityM2PerS) ||
        !finitePositive(fluid.thermalDiffusivityM2PerS) ||
        !finitePositive(fluid.prandtlNumber))
        return Result<NaturalConvectionResult>::failure(
            invalid("All fluid properties must be finite and positive.", "fluid"));
    if (fluid.sourceReference.empty() || request.geometrySourceReference.empty())
        return Result<NaturalConvectionResult>::failure(
            invalid("Fluid-property and exposed-geometry source references are required.", "sourceReferences"));
    if (!request.unobstructedQuiescentAmbient)
        return Result<NaturalConvectionResult>::failure(
            invalid("The vertical-plate correlation requires an explicitly confirmed unobstructed, quiescent ambient.",
                    "unobstructedQuiescentAmbient"));

    const double film = 0.5 * (surface + ambient);
    const double beta = 1.0 / film;
    const double deltaT = surface - ambient;
    const double rayleigh = 9.80665 * beta * deltaT *
        std::pow(length, 3.0) /
        (fluid.kinematicViscosityM2PerS * fluid.thermalDiffusivityM2PerS);
    if (!finitePositive(rayleigh) || rayleigh < 1.0e-1 || rayleigh > 1.0e12)
        return Result<NaturalConvectionResult>::failure(
            invalid("Rayleigh number is outside the supported Churchill-Chu vertical-plate range [1e-1, 1e12].",
                    "rayleighNumber"));

    const double pr = fluid.prandtlNumber;
    const double denominator = std::pow(1.0 + std::pow(0.492 / pr, 9.0 / 16.0), 8.0 / 27.0);
    const double nusselt = std::pow(0.825 + 0.387 * std::pow(rayleigh, 1.0 / 6.0) /
        denominator, 2.0);
    const double coefficient = nusselt * fluid.thermalConductivityWPerMK / length;
    const double resistance = 1.0 / (coefficient * area);
    if (!finitePositive(nusselt) || !finitePositive(coefficient) || !finitePositive(resistance))
        return Result<NaturalConvectionResult>::failure(
            invalid("Correlation produced a non-finite or non-positive result.", "correlationResult"));

    NaturalConvectionResult result;
    result.filmTemperatureK = film;
    result.rayleighNumber = rayleigh;
    result.nusseltNumber = nusselt;
    result.heatTransferCoefficientWPerM2K = coefficient;
    result.thermalResistanceKPerW = resistance;
    result.correlation = "Churchill-Chu vertical isothermal plate";
    result.status = "PRELIMINARY";
    result.explanation =
        "PRELIMINARY: isolated vertical-plate free-convection estimate using explicit film-temperature fluid properties. "
        "Not applicable to obstructed/enclosed winding surfaces; radiation, support conduction, inter-phase coupling "
        "and winding temperature gradients are excluded.";
    return Result<NaturalConvectionResult>::success(std::move(result));
}

} // namespace p135::thermal
