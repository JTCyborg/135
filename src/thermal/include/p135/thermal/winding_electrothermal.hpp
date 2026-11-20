#pragma once

#include "p135/core/result.hpp"
#include "p135/domain/winding_electrical.hpp"
#include "p135/thermal/steady_state.hpp"

#include <cstddef>
#include <string>

namespace p135::thermal {

struct WindingElectrothermalRequest {
    domain::WindingCopperLossRequest electrical;
    units::TemperatureKelvin ambientTemperature{293.15};
    double thermalResistanceKPerW{};
    units::TemperatureKelvin maximumWindingTemperature;
    std::string materialRevisionReference;
    std::string thermalResistanceSourceReference;
    std::string temperatureLimitSourceReference;
    double temperatureToleranceK{1.0e-3};
    double relaxationFactor{0.5};
    std::size_t maximumIterations{100};
};

struct WindingElectrothermalResult {
    AssessmentStatus status{AssessmentStatus::NotVerified};
    bool converged{false};
    std::size_t iterations{};
    units::TemperatureKelvin estimatedWindingTemperature;
    units::TemperatureDifference temperatureMargin;
    domain::WindingCopperLossResult copperLoss;
    double finalTemperatureResidualK{};
    std::string explanation;
};

// Iterates T = Tambient + Pcu(T) * Rtheta. This is a single lumped
// winding-to-ambient model and uses the DC-resistance copper-loss model;
// it does not include core loss, mutual thermal coupling, transients,
// skin/proximity effects, or spatial hot spots.
[[nodiscard]] Result<WindingElectrothermalResult> analyzeWindingElectrothermal(
    const WindingElectrothermalRequest& request);

} // namespace p135::thermal
