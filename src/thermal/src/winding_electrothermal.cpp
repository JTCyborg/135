#include "p135/thermal/winding_electrothermal.hpp"

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
        Severity::Error, "WINDING_ELECTROTHERMAL_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

Error nonConvergent(std::string message)
{
    Error error;
    error.code = ErrorCode::NonConvergence;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Warning, "WINDING_ELECTROTHERMAL_NON_CONVERGENCE",
        error.message, "maximumIterations", std::nullopt, std::nullopt
    });
    return error;
}

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

Result<WindingElectrothermalResult> analyzeWindingElectrothermal(
    const WindingElectrothermalRequest& request)
{
    const double ambient = request.ambientTemperature.si();
    const double maximum = request.maximumWindingTemperature.si();
    if (!finitePositive(ambient))
        return Result<WindingElectrothermalResult>::failure(
            invalid("Ambient temperature must be finite and above absolute zero.", "ambientTemperature"));
    if (!finitePositive(request.thermalResistanceKPerW))
        return Result<WindingElectrothermalResult>::failure(
            invalid("Winding-to-ambient thermal resistance must be finite and positive.", "thermalResistanceKPerW"));
    if (!finitePositive(maximum) || maximum <= ambient)
        return Result<WindingElectrothermalResult>::failure(
            invalid("Maximum winding temperature must be above ambient.", "maximumWindingTemperature"));
    if (!finitePositive(request.temperatureToleranceK) ||
        !std::isfinite(request.relaxationFactor) ||
        request.relaxationFactor <= 0.0 || request.relaxationFactor > 1.0 ||
        request.maximumIterations == 0)
        return Result<WindingElectrothermalResult>::failure(
            invalid("Tolerance and iteration count must be positive; relaxation factor must be in (0, 1].",
                    "iterationSettings"));
    if (request.materialRevisionReference.empty() ||
        request.thermalResistanceSourceReference.empty() ||
        request.temperatureLimitSourceReference.empty())
        return Result<WindingElectrothermalResult>::failure(
            invalid("Material revision, thermal resistance and temperature limit source references are required.",
                    "sourceReferences"));

    double temperature = ambient;
    WindingElectrothermalResult result;
    for (std::size_t iteration = 1; iteration <= request.maximumIterations; ++iteration) {
        auto electrical = request.electrical;
        electrical.windingTemperature = units::TemperatureKelvin{temperature};
        const auto loss = domain::calculateWindingCopperLoss(electrical);
        if (!loss)
            return Result<WindingElectrothermalResult>::failure(loss.error());

        const double target = ambient +
            loss.value().totalThreePhaseCopperLoss.si() * request.thermalResistanceKPerW;
        const double residual = target - temperature;
        if (!std::isfinite(target) || !std::isfinite(residual))
            return Result<WindingElectrothermalResult>::failure(
                invalid("Electrothermal iteration produced a non-finite temperature.", "temperature"));

        result.iterations = iteration;
        result.finalTemperatureResidualK = std::abs(residual);
        if (result.finalTemperatureResidualK <= request.temperatureToleranceK) {
            // Re-evaluate losses at the converged temperature so the returned
            // electrical and thermal values refer to the same operating point.
            const auto finalTemperature = target;
            electrical.windingTemperature = units::TemperatureKelvin{finalTemperature};
            const auto finalLoss = domain::calculateWindingCopperLoss(electrical);
            if (!finalLoss)
                return Result<WindingElectrothermalResult>::failure(finalLoss.error());
            const double finalTarget = ambient +
                finalLoss.value().totalThreePhaseCopperLoss.si() * request.thermalResistanceKPerW;
            result.finalTemperatureResidualK = std::abs(finalTarget - finalTemperature);
            result.converged = result.finalTemperatureResidualK <= request.temperatureToleranceK;
            result.estimatedWindingTemperature = units::TemperatureKelvin{finalTemperature};
            result.temperatureMargin = units::TemperatureDifference{maximum - finalTemperature};
            result.copperLoss = finalLoss.value();
            if (!result.converged) {
                result.status = AssessmentStatus::NotVerified;
                result.explanation = "NOT_VERIFIED: final loss re-evaluation moved the thermal operating point outside the convergence tolerance.";
                return Result<WindingElectrothermalResult>::success(std::move(result));
            }
            if (request.materialRevisionReference.empty() ||
                request.thermalResistanceSourceReference.empty() ||
                request.temperatureLimitSourceReference.empty()) {
                result.status = AssessmentStatus::NotVerified;
            } else {
                result.status = finalTemperature <= maximum
                    ? AssessmentStatus::Pass : AssessmentStatus::Fail;
            }
            result.explanation = result.status == AssessmentStatus::Pass
                ? "PASS: copper-loss and winding temperature iteration converged within the supplied temperature limit."
                : "FAIL: converged winding temperature exceeds the supplied limit.";
            return Result<WindingElectrothermalResult>::success(std::move(result));
        }
        temperature += request.relaxationFactor * residual;
    }

    result.converged = false;
    result.status = AssessmentStatus::NotVerified;
    result.estimatedWindingTemperature = units::TemperatureKelvin{temperature};
    result.temperatureMargin = units::TemperatureDifference{maximum - temperature};
    result.explanation = "NOT_VERIFIED: electrothermal iteration did not converge within the configured iteration limit.";
    return Result<WindingElectrothermalResult>::success(std::move(result));
}

} // namespace p135::thermal
