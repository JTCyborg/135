#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/component_catalog.hpp"
#include "p135/filter/harmonics.hpp"
#include "p135/thermal/steady_state.hpp"

#include <optional>
#include <string>
#include <vector>

namespace p135::thermal {

struct FilterCapacitorThermalAssignment {
    std::uint32_t branchId{};
    std::string capacitorComponentId;
    std::optional<double> thermalResistanceKPerW;
    std::string thermalResistanceSourceReference;
};

struct CapacitorThermalAssessment {
    std::uint32_t branchId{};
    std::string capacitorComponentId;
    double rippleCurrentRmsA{};
    std::optional<units::Current> ratedRippleCurrent;
    AssessmentStatus rippleCurrentStatus{AssessmentStatus::NotVerified};
    std::string rippleCurrentExplanation;
    std::optional<units::Power> esrLoss;
    ThermalNodeResult thermal;
};

struct CapacitorThermalResult {
    units::TemperatureKelvin ambientTemperature;
    std::vector<CapacitorThermalAssessment> capacitors;
};

[[nodiscard]] Result<CapacitorThermalResult> analyzeFilterCapacitorThermals(
    const filter::HarmonicAnalysisResult& harmonicResult,
    const std::vector<FilterCapacitorThermalAssignment>& assignments,
    const domain::CapacitorCatalog& capacitorCatalog,
    units::TemperatureKelvin ambientTemperature,
    const std::string& harmonicStudyRevision);

struct DampingResistorThermalAssignment {
    std::uint32_t branchId{};
    std::string resistorComponentId;
    /// Measured or calculated component-to-ambient Rtheta for the actual installation.
    std::optional<double> thermalResistanceKPerW;
    std::string thermalResistanceSourceReference;
};

/// Transfers harmonic I^2R losses into thermal nodes using the selected resistor
/// component's maximum temperature and immutable material-data provenance.
/// Missing installation-specific Rtheta remains NOT_VERIFIED.
[[nodiscard]] Result<SteadyStateResult> analyzeDampingResistorThermals(
    const filter::HarmonicAnalysisResult& harmonicResult,
    const std::vector<DampingResistorThermalAssignment>& assignments,
    const domain::ResistorCatalog& resistorCatalog,
    units::TemperatureKelvin ambientTemperature,
    const std::string& harmonicStudyRevision);

} // namespace p135::thermal
