#include "p135/thermal/steady_state.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>

namespace p135::thermal {
namespace {

Error invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "THERMAL_INPUT_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool finiteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

} // namespace

Result<SteadyStateResult> analyzeSteadyState(const SteadyStateRequest& request)
{
    const double ambient = request.ambientTemperature.si();
    if (!finitePositive(ambient))
        return Result<SteadyStateResult>::failure(
            invalid("Ambient temperature must be finite and above absolute zero.", "ambientTemperature"));
    if (request.nodes.empty())
        return Result<SteadyStateResult>::failure(
            invalid("At least one thermal node is required.", "nodes"));

    std::unordered_set<std::string> ids;
    SteadyStateResult result;
    result.ambientTemperature = request.ambientTemperature;
    result.nodes.reserve(request.nodes.size());

    for (std::size_t index = 0; index < request.nodes.size(); ++index) {
        const auto& node = request.nodes[index];
        const std::string field = "nodes[" + std::to_string(index) + "]";
        if (node.id.empty() || !ids.insert(node.id).second)
            return Result<SteadyStateResult>::failure(
                invalid("Thermal node IDs must be non-empty and unique.", field + ".id"));
        if (node.loss && !finiteNonNegative(node.loss->si()))
            return Result<SteadyStateResult>::failure(
                invalid("Dissipated power must be finite and non-negative.", field + ".loss"));
        if (node.thermalResistanceKPerW &&
            !finitePositive(*node.thermalResistanceKPerW))
            return Result<SteadyStateResult>::failure(
                invalid("Thermal resistance must be finite and greater than zero.", field + ".thermalResistanceKPerW"));
        if (node.maximumTemperature &&
            (!finitePositive(node.maximumTemperature->si()) ||
             node.maximumTemperature->si() <= ambient))
            return Result<SteadyStateResult>::failure(
                invalid("Maximum component temperature must be finite and above ambient.", field + ".maximumTemperature"));

        ThermalNodeResult nodeResult;
        nodeResult.id = node.id;
        nodeResult.label = node.label;
        nodeResult.loss = node.loss;

        if (!node.loss || !node.thermalResistanceKPerW || !node.maximumTemperature) {
            nodeResult.status = AssessmentStatus::NotVerified;
            nodeResult.explanation =
                "NOT_VERIFIED: loss, component-to-ambient thermal resistance, and maximum temperature are all required.";
        } else if (node.lossSourceReference.empty() ||
                   node.thermalResistanceSourceReference.empty() ||
                   node.temperatureLimitSourceReference.empty()) {
            nodeResult.status = AssessmentStatus::NotVerified;
            nodeResult.explanation =
                "NOT_VERIFIED: source references are required for loss, thermal resistance, and temperature limit.";
        } else {
            const double rise = node.loss->si() * *node.thermalResistanceKPerW;
            const double estimated = ambient + rise;
            const double margin = node.maximumTemperature->si() - estimated;
            if (!std::isfinite(estimated) || !std::isfinite(margin))
                return Result<SteadyStateResult>::failure(
                    invalid("Calculated temperature is outside the supported numeric domain.", field));
            nodeResult.estimatedTemperature = units::TemperatureKelvin{estimated};
            nodeResult.temperatureMargin = units::TemperatureDifference{margin};
            nodeResult.status = margin >= 0.0
                ? AssessmentStatus::Pass
                : AssessmentStatus::Fail;
            nodeResult.explanation = margin >= 0.0
                ? "PASS: estimated steady-state temperature is within the supplied limit."
                : "FAIL: estimated steady-state temperature exceeds the supplied limit.";
        }

        result.nodes.push_back(std::move(nodeResult));
    }

    return Result<SteadyStateResult>::success(std::move(result));
}

} // namespace p135::thermal
