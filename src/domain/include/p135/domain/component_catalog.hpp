#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/magnetic_product.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace p135::domain {

enum class CapacitorDielectric {
    PolypropyleneFilm, PolyesterFilm, CeramicClass1, CeramicClass2,
    AluminumElectrolytic, TantalumElectrolytic, Mica, Paper, Other
};

enum class CapacitorTechnology {
    MetallizedFilm, FilmFoil, CeramicMLCC, CeramicDisc,
    AluminumElectrolytic, TantalumElectrolytic, Other
};

/// Datasheet/measurement ESR point. Curves are interpolated log-log and never extrapolated.
struct CapacitorEsrPoint {
    units::Frequency frequency;
    units::Resistance esr;
};

/// A physical capacitor component. Optional frequency/thermal characteristics
/// remain unavailable until supported by a datasheet or measurement.
struct CapacitorComponent {
    std::string id;
    std::string manufacturer;
    std::string series;
    std::string partNumber;
    MaterialDataSource dataSource{MaterialDataSource::UserDefined};
    /// True only after the specific snapshot has passed source/data-quality review.
    bool evidenceVerified{false};
    std::string sourceReference;
    std::string datasetRevision;

    CapacitorDielectric dielectric{CapacitorDielectric::Other};
    CapacitorTechnology technology{CapacitorTechnology::Other};
    units::Capacitance capacitance;
    double toleranceFraction{}; // 0.05 means ±5%
    units::Voltage ratedVoltage; // DC or AC rating as identified by voltageRatingIsAC
    bool voltageRatingIsAC{false};

    std::optional<units::Resistance> esr;
    bool esrConfirmedFrequencyIndependent{false};
    std::vector<CapacitorEsrPoint> esrFrequencyCurve;
    std::optional<units::Inductance> esl;
    std::optional<double> dissipationFactor;
    std::optional<units::Current> rippleCurrent;
    units::TemperatureKelvin minimumTemperature;
    units::TemperatureKelvin maximumTemperature;
    std::optional<units::Time> ratedLife;
};

enum class ResistorTechnology {
    Wirewound, ThickFilm, ThinFilm, MetalFilm, MetalOxide,
    CarbonFilm, CarbonComposition, Foil, Cement, Other
};

struct ResistorComponent {
    std::string id;
    std::string manufacturer;
    std::string series;
    std::string partNumber;
    MaterialDataSource dataSource{MaterialDataSource::UserDefined};
    /// True only after the specific snapshot has passed source/data-quality review.
    bool evidenceVerified{false};
    std::string sourceReference;
    std::string datasetRevision;
    ResistorTechnology technology{ResistorTechnology::Other};
    units::Resistance resistance;
    double toleranceFraction{}; // 0.01 means ±1%
    units::Power ratedPower;
    units::Voltage maximumWorkingVoltage;
    std::optional<units::TemperatureCoefficient> temperatureCoefficient;
    std::optional<units::Inductance> parasiticInductance;
    std::optional<units::Capacitance> parasiticCapacitance;
    std::optional<double> singlePulseEnergyJoules;
    units::TemperatureKelvin minimumTemperature;
    units::TemperatureKelvin maximumTemperature;
};

[[nodiscard]] Result<void> validate(const CapacitorComponent& capacitor);
[[nodiscard]] Result<void> validate(const ResistorComponent& resistor);

/// Log-log interpolation between adjacent supplied ESR points. No extrapolation.
[[nodiscard]] Result<units::Resistance> interpolateCapacitorEsr(
    const CapacitorComponent& capacitor, units::Frequency frequency);

class CapacitorCatalog {
public:
    [[nodiscard]] Result<void> add(CapacitorComponent capacitor);
    [[nodiscard]] const CapacitorComponent* find(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return capacitors_.size(); }
private:
    std::unordered_map<std::string, CapacitorComponent> capacitors_;
};

class ResistorCatalog {
public:
    [[nodiscard]] Result<void> add(ResistorComponent resistor);
    [[nodiscard]] const ResistorComponent* find(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return resistors_.size(); }
private:
    std::unordered_map<std::string, ResistorComponent> resistors_;
};

} // namespace p135::domain
