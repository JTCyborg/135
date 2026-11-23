#include "p135/thermal/thermal_network.hpp"
#include "p135/thermal/natural_convection.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace p135::thermal {
namespace {

Error invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "THERMAL_NETWORK_INPUT_INVALID", error.message,
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

bool solveLinearSystem(std::vector<std::vector<double>>& matrix,
                       std::vector<double>& rhs)
{
    const std::size_t size = rhs.size();
    double matrixScale = 0.0;
    for (const auto& row : matrix)
        for (double value : row)
            matrixScale = std::max(matrixScale, std::abs(value));
    if (!finitePositive(matrixScale))
        return false;

    const double pivotTolerance =
        std::numeric_limits<double>::epsilon() * matrixScale *
        static_cast<double>(size) * 64.0;

    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                pivot = row;
        }
        if (!std::isfinite(matrix[pivot][column]) ||
            std::abs(matrix[pivot][column]) <= pivotTolerance)
            return false;

        if (pivot != column) {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(rhs[pivot], rhs[column]);
        }

        const double divisor = matrix[column][column];
        for (std::size_t j = column; j < size; ++j)
            matrix[column][j] /= divisor;
        rhs[column] /= divisor;

        for (std::size_t row = 0; row < size; ++row) {
            if (row == column)
                continue;
            const double factor = matrix[row][column];
            if (factor == 0.0)
                continue;
            for (std::size_t j = column; j < size; ++j)
                matrix[row][j] -= factor * matrix[column][j];
            rhs[row] -= factor * rhs[column];
        }
    }
    return std::all_of(rhs.begin(), rhs.end(),
                       [](double value) { return std::isfinite(value); });
}

} // namespace

Result<ThermalNetworkResult> analyzeThermalNetwork(
    const ThermalNetworkRequest& request)
{
    const double ambient = request.ambientTemperature.si();
    if (!finitePositive(ambient))
        return Result<ThermalNetworkResult>::failure(
            invalid("Ambient temperature must be finite and above absolute zero.",
                    "ambientTemperature"));
    if (request.nodes.empty())
        return Result<ThermalNetworkResult>::failure(
            invalid("At least one thermal network node is required.", "nodes"));

    const std::size_t count = request.nodes.size();
    std::unordered_map<std::string, std::size_t> indices;
    indices.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& node = request.nodes[i];
        const std::string field = "nodes[" + std::to_string(i) + "]";
        if (node.id.empty() || !indices.emplace(node.id, i).second)
            return Result<ThermalNetworkResult>::failure(
                invalid("Thermal node IDs must be non-empty and unique.", field + ".id"));
        if (!finiteNonNegative(node.loss.si()))
            return Result<ThermalNetworkResult>::failure(
                invalid("Node loss must be finite and non-negative.", field + ".loss"));
        if (node.maximumTemperature &&
            (!finitePositive(node.maximumTemperature->si()) ||
             node.maximumTemperature->si() <= ambient))
            return Result<ThermalNetworkResult>::failure(
                invalid("Maximum temperature must be finite and above ambient.",
                        field + ".maximumTemperature"));
    }

    std::vector<std::vector<double>> matrix(count, std::vector<double>(count, 0.0));
    std::vector<double> rhs(count, 0.0);
    std::vector<std::vector<std::size_t>> adjacency(count);
    std::vector<bool> hasAmbientPath(count, false);
    bool provenanceComplete = true;

    for (std::size_t i = 0; i < count; ++i) {
        rhs[i] = request.nodes[i].loss.si();
        if (request.nodes[i].lossSourceReference.empty() ||
            !request.nodes[i].maximumTemperature ||
            request.nodes[i].temperatureLimitSourceReference.empty())
            provenanceComplete = false;
    }

    for (std::size_t i = 0; i < request.links.size(); ++i) {
        const auto& link = request.links[i];
        const std::string field = "links[" + std::to_string(i) + "]";
        const auto first = indices.find(link.firstNodeId);
        const auto second = indices.find(link.secondNodeId);
        if (first == indices.end() || second == indices.end())
            return Result<ThermalNetworkResult>::failure(
                invalid("Thermal link references an unknown node.", field));
        if (first->second == second->second)
            return Result<ThermalNetworkResult>::failure(
                invalid("A thermal link must connect two different nodes.", field));
        if (!finitePositive(link.conductanceWPerK))
            return Result<ThermalNetworkResult>::failure(
                invalid("Thermal conductance must be finite and greater than zero.",
                        field + ".conductanceWPerK"));

        const std::size_t a = first->second;
        const std::size_t b = second->second;
        const double conductance = link.conductanceWPerK;
        matrix[a][a] += conductance;
        matrix[b][b] += conductance;
        matrix[a][b] -= conductance;
        matrix[b][a] -= conductance;
        adjacency[a].push_back(b);
        adjacency[b].push_back(a);
        // Natural-convection links remain preliminary even if a caller
        // clears the convenience flag after creating the link.
        const bool naturalConvectionLink =
            link.sourceReference.starts_with("natural-convection;");
        if (link.sourceReference.empty() || link.preliminaryModel ||
            naturalConvectionLink)
            provenanceComplete = false;
    }

    for (std::size_t i = 0; i < request.ambientLinks.size(); ++i) {
        const auto& link = request.ambientLinks[i];
        const std::string field = "ambientLinks[" + std::to_string(i) + "]";
        const auto node = indices.find(link.nodeId);
        if (node == indices.end())
            return Result<ThermalNetworkResult>::failure(
                invalid("Ambient link references an unknown node.", field + ".nodeId"));
        if (!finitePositive(link.conductanceWPerK))
            return Result<ThermalNetworkResult>::failure(
                invalid("Ambient conductance must be finite and greater than zero.",
                        field + ".conductanceWPerK"));
        const std::size_t index = node->second;
        matrix[index][index] += link.conductanceWPerK;
        rhs[index] += link.conductanceWPerK * ambient;
        hasAmbientPath[index] = true;
        // Natural-convection adapter links remain preliminary even if a
        // caller clears the public convenience flag after creating the link.
        const bool naturalConvectionBoundary =
            link.sourceReference.starts_with("natural-convection;");
        if (link.sourceReference.empty() || link.preliminaryModel ||
            naturalConvectionBoundary)
            provenanceComplete = false;
    }

    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(rhs[i]) ||
            !std::all_of(matrix[i].begin(), matrix[i].end(),
                         [](double value) { return std::isfinite(value); }))
            return Result<ThermalNetworkResult>::failure(
                invalid("Thermal network coefficients overflow the supported numeric range.",
                        "network"));
    }

    // Every connected component must have at least one path to ambient.
    std::vector<bool> visited(count, false);
    for (std::size_t start = 0; start < count; ++start) {
        if (visited[start])
            continue;
        std::vector<std::size_t> stack{start};
        visited[start] = true;
        bool componentHasAmbient = false;
        while (!stack.empty()) {
            const auto current = stack.back();
            stack.pop_back();
            componentHasAmbient = componentHasAmbient || hasAmbientPath[current];
            for (const auto neighbour : adjacency[current]) {
                if (!visited[neighbour]) {
                    visited[neighbour] = true;
                    stack.push_back(neighbour);
                }
            }
        }
        if (!componentHasAmbient)
            return Result<ThermalNetworkResult>::failure(
                invalid("Every connected thermal component must have a conductive path to ambient.",
                        "ambientLinks"));
    }

    if (!solveLinearSystem(matrix, rhs))
        return Result<ThermalNetworkResult>::failure(
            invalid("Thermal network matrix is singular or numerically ill-conditioned.",
                    "network"));

    ThermalNetworkResult result;
    result.ambientTemperature = request.ambientTemperature;
    result.solved = true;
    result.modelProvenanceComplete = provenanceComplete;
    result.nodes.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        const auto& input = request.nodes[i];
        const double temperature = rhs[i];
        if (!finitePositive(temperature))
            return Result<ThermalNetworkResult>::failure(
                invalid("Calculated absolute temperature is outside the supported numeric domain.",
                        "nodes[" + std::to_string(i) + "]"));

        ThermalNetworkNodeResult node;
        node.id = input.id;
        node.label = input.label;
        node.loss = input.loss;
        node.estimatedTemperature = units::TemperatureKelvin{temperature};

        if (input.maximumTemperature) {
            const double margin = input.maximumTemperature->si() - temperature;
            if (!std::isfinite(margin))
                return Result<ThermalNetworkResult>::failure(
                    invalid("Calculated temperature margin is not finite.",
                            "nodes[" + std::to_string(i) + "]"));
            node.temperatureMargin = units::TemperatureDifference{margin};
        }

        const bool nodeProvenanceComplete =
            !input.lossSourceReference.empty() &&
            input.maximumTemperature.has_value() &&
            !input.temperatureLimitSourceReference.empty() &&
            provenanceComplete;
        if (!nodeProvenanceComplete) {
            node.status = AssessmentStatus::NotVerified;
            node.explanation =
                "NOT_VERIFIED: loss, network conductances, and temperature limit must have traceable source data.";
        } else if (node.temperatureMargin->si() >= 0.0) {
            node.status = AssessmentStatus::Pass;
            node.explanation =
                "PASS: solved steady-state node temperature is within the supplied limit.";
        } else {
            node.status = AssessmentStatus::Fail;
            node.explanation =
                "FAIL: solved steady-state node temperature exceeds the supplied limit.";
        }
        result.nodes.push_back(std::move(node));
    }

    return Result<ThermalNetworkResult>::success(std::move(result));
}


Result<ThermalAmbientLink> makeNaturalConvectionAmbientLink(
    const std::string& nodeId,
    const NaturalConvectionResult& convection,
    const std::string& boundaryModelSourceReference)
{
    if (nodeId.empty() || boundaryModelSourceReference.empty() ||
        !std::isfinite(convection.thermalResistanceKPerW) ||
        convection.thermalResistanceKPerW <= 0.0 ||
        convection.geometrySourceReference.empty() ||
        convection.fluidPropertiesSourceReference.empty() ||
        convection.correlation.empty() || convection.status.empty()) {
        return Result<ThermalAmbientLink>::failure(
            invalid("Natural-convection boundary requires a node, positive resistance, correlation status, "
                    "and traceable geometry, fluid-property and boundary-model references.",
                    "naturalConvectionBoundary"));
    }

    const double conductance = 1.0 / convection.thermalResistanceKPerW;
    if (!finitePositive(conductance))
        return Result<ThermalAmbientLink>::failure(
            invalid("Natural-convection resistance cannot be converted to a finite positive conductance.",
                    "naturalConvectionBoundary.thermalResistanceKPerW"));

    ThermalAmbientLink link;
    link.nodeId = nodeId;
    link.conductanceWPerK = conductance;
    link.sourceReference =
        "natural-convection;correlation=" + convection.correlation +
        ";status=" + convection.status +
        ";geometry=" + convection.geometrySourceReference +
        ";fluid=" + convection.fluidPropertiesSourceReference +
        ";boundary=" + boundaryModelSourceReference;
    // Current natural-convection correlations are research estimates only;
    // caller-supplied status text cannot promote them to a verified boundary.
    link.preliminaryModel = true;
    return Result<ThermalAmbientLink>::success(std::move(link));
}

} // namespace p135::thermal
