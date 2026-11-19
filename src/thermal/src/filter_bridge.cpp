#include "p135/thermal/filter_bridge.hpp"

#include <algorithm>

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

Result<CapacitorThermalResult> analyzeFilterCapacitorThermals(
    const filter::HarmonicAnalysisResult& harmonicResult,
    const std::vector<FilterCapacitorThermalAssignment>& assignments,
    const domain::CapacitorCatalog& capacitorCatalog,
    units::TemperatureKelvin ambientTemperature,
    const std::string& harmonicStudyRevision)
{
    if (assignments.empty())
        return Result<CapacitorThermalResult>::failure(
            invalid("At least one filter-capacitor assignment is required.", "assignments"));
    if (harmonicStudyRevision.empty())
        return Result<CapacitorThermalResult>::failure(
            invalid("A harmonic-study revision is required for loss provenance.", "harmonicStudyRevision"));

    CapacitorThermalResult result;
    result.ambientTemperature = ambientTemperature;
    result.capacitors.reserve(assignments.size());

    std::unordered_set<std::uint32_t> assignedBranches;
    std::unordered_set<std::string> assignedComponents;
    for (std::size_t index = 0; index < assignments.size(); ++index) {
        const auto& assignment = assignments[index];
        const std::string field = "assignments[" + std::to_string(index) + "]";
        if (assignment.branchId == 0 || !assignedBranches.insert(assignment.branchId).second)
            return Result<CapacitorThermalResult>::failure(
                invalid("Branch IDs in capacitor assignments must be non-zero and unique.", field + ".branchId"));
        if (assignment.capacitorComponentId.empty() ||
            !assignedComponents.insert(assignment.capacitorComponentId).second)
            return Result<CapacitorThermalResult>::failure(
                invalid("Capacitor component IDs must be non-empty and unique.", field + ".capacitorComponentId"));

        const auto stress = std::find_if(
            harmonicResult.branchStresses.begin(), harmonicResult.branchStresses.end(),
            [&assignment](const auto& item) { return item.branchId == assignment.branchId; });
        if (stress == harmonicResult.branchStresses.end())
            return Result<CapacitorThermalResult>::failure(
                invalid("No harmonic stress result exists for the assigned filter branch.", field + ".branchId"));

        const auto* capacitor = capacitorCatalog.find(assignment.capacitorComponentId);
        if (!capacitor)
            return Result<CapacitorThermalResult>::failure(
                invalid("Assigned capacitor is not present in the component catalog.", field + ".capacitorComponentId"));
        const auto capacitorValidation = domain::validate(*capacitor);
        if (!capacitorValidation)
            return Result<CapacitorThermalResult>::failure(capacitorValidation.error());

        bool esrCoverageComplete = true;
        double lossW = 0.0;
        std::size_t branchPointCount = 0;
        for (const auto& point : harmonicResult.branchPoints) {
            if (point.branchId != assignment.branchId)
                continue;
            ++branchPointCount;
            const double current = point.capacitorCurrentRmsA;
            if (!std::isfinite(current) || current < 0.0)
                return Result<CapacitorThermalResult>::failure(
                    invalid("Harmonic branch current is invalid.", field + ".branchPoints"));
            if (current == 0.0)
                continue;

            const auto esr = domain::interpolateCapacitorEsr(*capacitor, point.frequency);
            if (!esr) {
                esrCoverageComplete = false;
                continue;
            }
            lossW += current * current * esr.value().si();
            if (!std::isfinite(lossW))
                return Result<CapacitorThermalResult>::failure(
                    invalid("Accumulated capacitor ESR loss is outside the supported numeric domain.",
                            field + ".esrLoss"));
        }

        const bool hasBranchPoints = branchPointCount > 0;
        const bool lossAvailable = hasBranchPoints && esrCoverageComplete;
        const double rmsCurrent = stress->totalRmsCurrentA;
        if (!std::isfinite(rmsCurrent) || rmsCurrent < 0.0)
            return Result<CapacitorThermalResult>::failure(
                invalid("Aggregate capacitor RMS current is invalid.", field + ".rippleCurrent"));

        const bool componentEvidenceVerified = capacitor->evidenceVerified &&
            (capacitor->dataSource == domain::MaterialDataSource::ManufacturerDatasheet ||
             capacitor->dataSource == domain::MaterialDataSource::Measurement);

        CapacitorThermalAssessment assessment;
        assessment.branchId = assignment.branchId;
        assessment.capacitorComponentId = capacitor->id;
        assessment.rippleCurrentRmsA = rmsCurrent;
        assessment.ratedRippleCurrent = capacitor->rippleCurrent;

        if (!componentEvidenceVerified) {
            assessment.rippleCurrentStatus = AssessmentStatus::NotVerified;
            assessment.rippleCurrentExplanation =
                "NOT_VERIFIED: component values are user-defined and have not been verified against a datasheet or measurement.";
        } else if (!capacitor->rippleCurrent || capacitor->rippleCurrent->si() <= 0.0) {
            assessment.rippleCurrentStatus = AssessmentStatus::NotVerified;
            assessment.rippleCurrentExplanation =
                "NOT_VERIFIED: a positive datasheet/measurement ripple-current rating is required.";
        } else if (rmsCurrent <= capacitor->rippleCurrent->si()) {
            assessment.rippleCurrentStatus = AssessmentStatus::Pass;
            assessment.rippleCurrentExplanation =
                "PASS: aggregate RMS current does not exceed the supplied ripple-current rating. Frequency/temperature correction factors are not included.";
        } else {
            assessment.rippleCurrentStatus = AssessmentStatus::Fail;
            assessment.rippleCurrentExplanation =
                "FAIL: aggregate RMS current exceeds the supplied ripple-current rating.";
        }

        ThermalNodeInput thermalNode;
        thermalNode.id = capacitor->id;
        thermalNode.label = capacitor->manufacturer + " " + capacitor->partNumber;
        if (lossAvailable) {
            thermalNode.loss = units::Power{lossW};
            assessment.esrLoss = units::Power{lossW};
        }
        thermalNode.thermalResistanceKPerW = assignment.thermalResistanceKPerW;
        thermalNode.maximumTemperature = capacitor->maximumTemperature;
        thermalNode.lossSourceReference = "harmonic-study:" + harmonicStudyRevision +
            ";filter-branch:" + std::to_string(assignment.branchId) +
            ";capacitor:" + capacitor->id + "@" + capacitor->datasetRevision +
            ";ESR-model:frequency-curve-or-confirmed-constant";
        thermalNode.thermalResistanceSourceReference = assignment.thermalResistanceSourceReference;
        thermalNode.temperatureLimitSourceReference = capacitor->sourceReference +
            " (dataset revision " + capacitor->datasetRevision + ")";

        if (!lossAvailable || !componentEvidenceVerified) {
            thermalNode.lossSourceReference.clear();
            thermalNode.temperatureLimitSourceReference.clear();
        }

        SteadyStateRequest singleNodeRequest;
        singleNodeRequest.ambientTemperature = ambientTemperature;
        singleNodeRequest.nodes.push_back(std::move(thermalNode));
        const auto thermalResult = analyzeSteadyState(singleNodeRequest);
        if (!thermalResult)
            return Result<CapacitorThermalResult>::failure(thermalResult.error());

        assessment.thermal = thermalResult.value().nodes.front();
        if (!hasBranchPoints)
            assessment.thermal.explanation =
                "NOT_VERIFIED: harmonic result contains no per-harmonic current data for this branch.";
        else if (!esrCoverageComplete)
            assessment.thermal.explanation =
                "NOT_VERIFIED: ESR data does not cover every non-zero-current harmonic frequency; extrapolation is not permitted.";
        result.capacitors.push_back(std::move(assessment));
    }

    return Result<CapacitorThermalResult>::success(std::move(result));
}

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
        if (!resistor->evidenceVerified ||
            resistor->dataSource == domain::MaterialDataSource::UserDefined)
            node.temperatureLimitSourceReference.clear();
        thermalRequest.nodes.push_back(std::move(node));
    }

    return analyzeSteadyState(thermalRequest);
}

} // namespace p135::thermal
