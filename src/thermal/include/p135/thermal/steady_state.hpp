#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <optional>
#include <string>
#include <vector>

namespace p135::thermal {

enum class AssessmentStatus { Pass, Fail, NotVerified };

struct ThermalNodeInput {
    std::string id;
    std::string label;
    /// Total steady-state dissipated power in this component, W.
    std::optional<units::Power> loss;
    /// Component-to-ambient thermal resistance, K/W. Must match the mounting/cooling arrangement.
    std::optional<double> thermalResistanceKPerW;
    /// Maximum permitted component temperature from its datasheet/design requirement, K absolute.
    std::optional<units::TemperatureKelvin> maximumTemperature;
    std::string lossSourceReference;
    std::string thermalResistanceSourceReference;
    std::string temperatureLimitSourceReference;
};

struct SteadyStateRequest {
    units::TemperatureKelvin ambientTemperature{293.15};
    std::vector<ThermalNodeInput> nodes;
};

struct ThermalNodeResult {
    std::string id;
    std::string label;
    AssessmentStatus status{AssessmentStatus::NotVerified};
    std::optional<units::TemperatureKelvin> estimatedTemperature;
    std::optional<units::TemperatureDifference> temperatureMargin;
    std::optional<units::Power> loss;
    std::string explanation;
};

struct SteadyStateResult {
    units::TemperatureKelvin ambientTemperature;
    std::vector<ThermalNodeResult> nodes;
};

/// Independent, single-node steady-state estimate: T = Tambient + Ploss * Rtheta.
/// Missing loss, thermal resistance, or temperature limit yields NOT_VERIFIED.
/// This first version does not model mutual thermal coupling, transients, or spatial hot spots.
[[nodiscard]] Result<SteadyStateResult> analyzeSteadyState(
    const SteadyStateRequest& request);

} // namespace p135::thermal
