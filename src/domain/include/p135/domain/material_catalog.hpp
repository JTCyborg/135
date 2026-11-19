#pragma once

#include "p135/domain/magnetic_product.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace p135::domain {

enum class InsulationThermalClass { NotSpecified, A, E, B, F, H, N, R, C };

struct ConductorMaterial {
    std::string id;
    std::string manufacturer;
    std::string grade;
    MaterialDataSource dataSource{MaterialDataSource::UserDefined};
    std::string sourceReference;
    std::string datasetRevision;
    units::Resistivity resistivityAt20C;
    units::TemperatureCoefficient temperatureCoefficient;
    units::Density density;
};

struct InsulationMaterial {
    std::string id;
    std::string manufacturer;
    std::string grade;
    MaterialDataSource dataSource{MaterialDataSource::UserDefined};
    std::string sourceReference;
    std::string datasetRevision;
    InsulationThermalClass thermalClass{InsulationThermalClass::NotSpecified};
    units::TemperatureKelvin maximumTemperature;
    units::ElectricFieldStrength dielectricStrength;
    // Optional properties remain explicitly unavailable when not supplied.
    double relativePermittivity{};
    units::Density density;
};

/// Missing loss data means core-loss calculations are NOT_VERIFIED, never zero-loss.
[[nodiscard]] Result<void> validate(const CoreMaterial& material);
[[nodiscard]] Result<void> validate(const ConductorMaterial& material);
[[nodiscard]] Result<void> validate(const InsulationMaterial& material);

class CoreMaterialCatalog {
public:
    [[nodiscard]] Result<void> add(CoreMaterial material);
    [[nodiscard]] const CoreMaterial* find(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return materials_.size(); }
private:
    std::unordered_map<std::string, CoreMaterial> materials_;
};

class ConductorMaterialCatalog {
public:
    [[nodiscard]] Result<void> add(ConductorMaterial material);
    [[nodiscard]] const ConductorMaterial* find(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return materials_.size(); }
private:
    std::unordered_map<std::string, ConductorMaterial> materials_;
};

class InsulationMaterialCatalog {
public:
    [[nodiscard]] Result<void> add(InsulationMaterial material);
    [[nodiscard]] const InsulationMaterial* find(const std::string& id) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept { return materials_.size(); }
private:
    std::unordered_map<std::string, InsulationMaterial> materials_;
};

} // namespace p135::domain
