#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/thermal/steady_state.hpp"

#include <optional>
#include <string>
#include <vector>

namespace p135::thermal {

struct NaturalConvectionResult;

struct ThermalNetworkNodeInput {
    std::string id;
    std::string label;
    units::Power loss;
    std::optional<units::TemperatureKelvin> maximumTemperature;
    std::string lossSourceReference;
    std::string temperatureLimitSourceReference;
};

/// Symmetric conductive thermal coupling between two modeled nodes, W/K.
struct ThermalConductanceLink {
    std::string firstNodeId;
    std::string secondNodeId;
    double conductanceWPerK{};
    std::string sourceReference;
    bool preliminaryModel{false};
};

/// Conductive/linearized thermal path from a modeled node to ambient, W/K.
struct ThermalAmbientLink {
    std::string nodeId;
    double conductanceWPerK{};
    std::string sourceReference;
    bool preliminaryModel{false};
};

struct ThermalNetworkRequest {
    units::TemperatureKelvin ambientTemperature{293.15};
    std::vector<ThermalNetworkNodeInput> nodes;
    std::vector<ThermalConductanceLink> links;
    std::vector<ThermalAmbientLink> ambientLinks;
};

struct ThermalNetworkNodeResult {
    std::string id;
    std::string label;
    AssessmentStatus status{AssessmentStatus::NotVerified};
    units::TemperatureKelvin estimatedTemperature;
    std::optional<units::TemperatureDifference> temperatureMargin;
    units::Power loss;
    std::string explanation;
};

struct ThermalNetworkResult {
    units::TemperatureKelvin ambientTemperature;
    std::vector<ThermalNetworkNodeResult> nodes;
    bool solved{false};
    bool modelProvenanceComplete{false};
};

/// Solves a linear steady-state conductance network:
/// sum_j G_ij (T_i - T_j) + G_i,a (T_i - T_a) = P_i.
/// All conductances are explicit inputs; no geometry or cooling values are inferred.
/// The model is linear and does not include radiation, nonlinear convection,
/// transient storage, or spatial temperature gradients within a node.
[[nodiscard]] Result<ThermalNetworkResult> analyzeThermalNetwork(
    const ThermalNetworkRequest& request);

/// Converts a natural-convection resistance into a node-to-ambient conductance.
/// The correlation's PRELIMINARY status is carried into the network and prevents
/// a PASS assessment until the boundary model is independently verified.
[[nodiscard]] Result<ThermalAmbientLink> makeNaturalConvectionAmbientLink(
    const std::string& nodeId,
    const NaturalConvectionResult& convection,
    const std::string& boundaryModelSourceReference);

} // namespace p135::thermal
