#include "p135/thermal/filter_bridge.hpp"

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
        Severity::Error, "THERMAL_FILTER_BRIDGE_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

} // namespace

Result<SteadyStateResult> analyzeDampingResistorThermals(
    const filter::HarmonicAnalysisResult& harmonicResult,
    const std::vector<DampingResistorThermalAssignment>& assignments,
    const domain::ResistorCatalog& resistorCatalog,
    units::TemperatureKelvin ambientTemperature,
    const std::string& harmonicStudyRevision)
{
    if (assignments.empty())
        return Result<SteadyStateResult>::failure(
            invalid("At least one damping-resistor assignment is required.", "assignments"));
    if (harmonicStudyRevision.empty())
        return Result<SteadyStateResult>::failure(
            invalid("A harmonic-study revision is required for loss provenance.", "harmonicStudyRevision"));

    SteadyStateRequest thermalRequest;
    thermalRequest.ambientTemperature = ambientTemperature;
    thermalRequest.nodes.reserve(assignments.size());

    std::unordered_set<std::uint32_t> assignedBranches;
    std::unordered_set<std::string> assignedComponents;

    for (std::size_t index = 0; index < assignments.size(); ++index) {
        const auto& assignment = assignments[index];
        const std::string field = "assignments[" + std::to_string(index) + "]";
        if (assignment.branchId == 0 ||
            !assignedBranches.insert(assignment.branchId).second)
            return Result<SteadyStateResult>::failure(
                invalid("Branch IDs in thermal assignments must be non-zero and unique.", field + ".branchId"));
        if (assignment.resistorComponentId.empty() ||
            !assignedComponents.insert(assignment.resistorComponentId).second)
            return Result<SteadyStateResult>::failure(
                invalid("Resistor component IDs must be non-empty and unique.", field + ".resistorComponentId"));

        const auto stress = std::find_if(
            harmonicResult.branchStresses.begin(), harmonicResult.branchStresses.end(),
            [&assignment](const auto& item) { return item.branchId == assignment.branchId; });
        if (stress == harmonicResult.branchStresses.end())
            return Result<SteadyStateResult>::failure(
                invalid("No harmonic stress result exists for the assigned filter branch.", field + ".branchId"));

        const auto* resistor = resistorCatalog.find(assignment.resistorComponentId);
        if (!resistor)
            return Result<SteadyStateResult>::failure(
                invalid("Assigned damping resistor is not present in the component catalog.", field + ".resistorComponentId"));
        const auto resistorValidation = domain::validate(*resistor);
        if (!resistorValidation)
            return Result<SteadyStateResult>::failure(resistorValidation.error());

        ThermalNodeInput node;
        node.id = resistor->id;
        node.label = resistor->manufacturer + " " + resistor->partNumber;
        node.loss = units::Power{stress->dampingResistorLossW};
        node.thermalResistanceKPerW = assignment.thermalResistanceKPerW;
        node.maximumTemperature = resistor->maximumTemperature;
        node.lossSourceReference = "harmonic-study:" + harmonicStudyRevision +
            ";filter-branch:" + std::to_string(assignment.branchId) +
            ";resistor:" + resistor->id + "@" + resistor->datasetRevision;
        node.thermalResistanceSourceReference = assignment.thermalResistanceSourceReference;
        node.temperatureLimitSourceReference = resistor->sourceReference +
            " (dataset revision " + resistor->datasetRevision + ")";
        thermalRequest.nodes.push_back(std::move(node));
    }

    return analyzeSteadyState(thermalRequest);
}

} // namespace p135::thermal
