#include "p135/thermal/natural_convection.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace p135::thermal {
namespace {

Error invalidHorizontal(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "HORIZONTAL_NATURAL_CONVECTION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

bool finitePositiveHorizontal(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

Result<NaturalConvectionResult> analyzeHorizontalPlateNaturalConvection(
    const HorizontalPlateNaturalConvectionRequest& request)
{
    const double area = request.exposedArea.si();
    const double perimeter = request.exposedPerimeter.si();
    const double ambient = request.ambientTemperature.si();
    const double surface = request.surfaceTemperature.si();
    const auto& fluid = request.fluid;

    if (!finitePositiveHorizontal(area) || !finitePositiveHorizontal(perimeter))
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("Exposed area and perimeter must be finite and positive.",
                              "exposedGeometry"));
    if (!finitePositiveHorizontal(ambient) || !finitePositiveHorizontal(surface) ||
        surface <= ambient)
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("Surface temperature must be finite and above ambient.",
                              "surfaceTemperature"));
    if (!finitePositiveHorizontal(fluid.thermalConductivityWPerMK) ||
        !finitePositiveHorizontal(fluid.kinematicViscosityM2PerS) ||
        !finitePositiveHorizontal(fluid.thermalDiffusivityM2PerS) ||
        !finitePositiveHorizontal(fluid.prandtlNumber))
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("All fluid properties must be finite and positive.", "fluid"));
    const double derivedPrandtl =
        fluid.kinematicViscosityM2PerS / fluid.thermalDiffusivityM2PerS;
    if (std::abs(derivedPrandtl - fluid.prandtlNumber) >
        0.05 * fluid.prandtlNumber)
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("Prandtl number is inconsistent with viscosity / diffusivity by more than 5%.",
                              "fluid.prandtlNumber"));
    if (fluid.sourceReference.empty() || request.geometrySourceReference.empty())
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("Fluid-property and exposed-geometry source references are required.",
                              "sourceReferences"));
    if (!request.unobstructedQuiescentAmbient)
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("The correlation requires explicitly confirmed unobstructed, quiescent ambient.",
                              "unobstructedQuiescentAmbient"));

    const double characteristicLength = area / perimeter;
    if (!finitePositiveHorizontal(characteristicLength))
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("Derived horizontal-plate characteristic length is invalid.",
                              "characteristicLength"));

    const double film = 0.5 * (surface + ambient);
    const double rayleigh = 9.80665 * (1.0 / film) * (surface - ambient) *
        std::pow(characteristicLength, 3.0) /
        (fluid.kinematicViscosityM2PerS * fluid.thermalDiffusivityM2PerS);
    if (!finitePositiveHorizontal(rayleigh))
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("Rayleigh number is not finite and positive.", "rayleighNumber"));

    double nusselt = 0.0;
    if (request.orientation == HorizontalPlateOrientation::HeatedFaceUpward) {
        if (rayleigh >= 1.0e4 && rayleigh < 1.0e7)
            nusselt = 0.54 * std::pow(rayleigh, 0.25);
        else if (rayleigh >= 1.0e7 && rayleigh <= 1.0e11)
            nusselt = 0.15 * std::pow(rayleigh, 1.0 / 3.0);
        else
            return Result<NaturalConvectionResult>::failure(
                invalidHorizontal("Upward-facing heated plate correlation supports Ra in [1e4, 1e11].",
                                  "rayleighNumber"));
    } else {
        if (rayleigh < 1.0e5 || rayleigh > 1.0e10)
            return Result<NaturalConvectionResult>::failure(
                invalidHorizontal("Downward-facing heated plate correlation supports Ra in [1e5, 1e10].",
                                  "rayleighNumber"));
        nusselt = 0.27 * std::pow(rayleigh, 0.25);
    }

    const double coefficient =
        nusselt * fluid.thermalConductivityWPerMK / characteristicLength;
    const double resistance = 1.0 / (coefficient * area);
    if (!finitePositiveHorizontal(nusselt) ||
        !finitePositiveHorizontal(coefficient) ||
        !finitePositiveHorizontal(resistance))
        return Result<NaturalConvectionResult>::failure(
            invalidHorizontal("Correlation produced a non-finite or non-positive result.",
                              "correlationResult"));

    NaturalConvectionResult result;
    result.filmTemperatureK = film;
    result.rayleighNumber = rayleigh;
    result.nusseltNumber = nusselt;
    result.heatTransferCoefficientWPerM2K = coefficient;
    result.thermalResistanceKPerW = resistance;
    result.correlation = request.orientation ==
        HorizontalPlateOrientation::HeatedFaceUpward
        ? "McAdams horizontal plate, heated face upward"
        : "McAdams horizontal plate, heated face downward";
    result.geometrySourceReference = request.geometrySourceReference;
    result.fluidPropertiesSourceReference = fluid.sourceReference;
    result.status = "PRELIMINARY";
    result.explanation =
        "PRELIMINARY: horizontal-plate free-convection estimate using explicit area, perimeter and film-temperature "
        "fluid properties. Does not account for winding enclosure, adjacent phases, edge effects, radiation, "
        "supports, forced airflow or non-isothermal surfaces.";
    return Result<NaturalConvectionResult>::success(std::move(result));
}

} // namespace p135::thermal
