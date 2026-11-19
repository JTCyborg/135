#pragma once

#include "p135/domain/magnetic_product.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace p135::domain {

enum class CapacitorDielectric {
    PolypropyleneFilm, PolyesterFilm, CeramicClass1, CeramicClass2,
    AluminumElectrolytic, TantalumElectrolytic, Mica, Paper, Other
};

enum class CapacitorTechnology {
    MetallizedFilm, FilmFoil, CeramicMLCC, CeramicDisc,
    AluminumElectrolytic, TantalumElectrolytic, Other
};

/// A physical capacitor component. Optional frequency/thermal characteristics
/// remain unavailable until supported by a datasheet or measurement.
struct CapacitorComponent {
    std::string id;
    std::string manufacturer;
    std::string series;
    std::string partNumber;
    MaterialDataSource dataSource{MaterialDataSource::UserDefined};
    std::string sourceReference;
    std::string datasetRevision;

    CapacitorDielectric dielectric{CapacitorDielectric::Other};
    CapacitorTechnology technology{CapacitorTechnology::Other};
    units::Capacitance capacitance;
    double toleranceFraction{}; // 0.05 means ±5%
    units::Voltage ratedVoltage; // DC or AC rating as identified by voltageRatingIsAC
    bool voltageRatingIsAC{false};

    std::optional<units::Resistance> esr;
    std::optional<units::Inductance> esl;
    std::optional<double> dissipationFactor;
    std::optional<units::Current> rippleCurrent;
    units::TemperatureKelvin minimumTemperature;
    units::TemperatureKelvin maximumTemperature;
    std::optional<units::Time> ratedLife;
};

[[nodiscard]] Result<void> validate(const CapacitorComponent& capacitor);

class CapacitorCatalog {
public:
    [[nodiscard]] Result<void> add(CapacitorComponent capacitor);
    [[nodiscard]] const CapacitorComponent* find(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return capacitors_.size(); }
private:
    std::unordered_map<std::string, CapacitorComponent> capacitors_;
};

} // namespace p135::domain
