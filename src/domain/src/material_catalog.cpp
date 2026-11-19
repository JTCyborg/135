#include "p135/domain/material_catalog.hpp"

#include <cmath>
#include <utility>

namespace p135::domain {
namespace {
Error materialError(ErrorCode code, std::string message, std::string field,
                    const char* diagnosticCode)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, diagnosticCode, error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}
bool finitePositive(double value) { return std::isfinite(value) && value > 0.0; }
bool validSource(MaterialDataSource source)
{
    return source == MaterialDataSource::ManufacturerDatasheet ||
           source == MaterialDataSource::Measurement ||
           source == MaterialDataSource::UserDefined;
}
template <class T>
Result<void> addUnique(std::unordered_map<std::string, T>& items, T item,
                       const char* diagnosticCode, const char* label)
{
    if (items.contains(item.id))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput, std::string(label) + " with this ID already exists.",
            "id", diagnosticCode));
    const auto id = item.id;
    items.emplace(id, std::move(item));
    return Result<void>::success();
}
} // namespace

Result<void> validate(const CoreMaterial& material)
{
    if (material.id.empty() || material.grade.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData, "Material ID and grade are required.", "material",
            "CORE_MATERIAL_INVALID"));
    if (material.dataSource == MaterialDataSource::ManufacturerDatasheet &&
        material.manufacturer.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData,
            "Manufacturer is required for manufacturer-datasheet material data.", "manufacturer",
            "CORE_MATERIAL_INVALID"));
    if (!validSource(material.dataSource))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput, "Unsupported material data source.", "dataSource",
            "CORE_MATERIAL_INVALID"));
    if (!finitePositive(material.density.si()))
        return Result<void>::failure(materialError(
            ErrorCode::MissingMaterialProperty, "Material density must be positive.", "density",
            "CORE_MATERIAL_INVALID"));
    if (material.datasetRevision.empty() || material.sourceReference.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData, "Data revision and provenance reference are required.",
            "provenance", "CORE_MATERIAL_INVALID"));
    if (material.bhCurve.size() < 2)
        return Result<void>::failure(materialError(
            ErrorCode::MissingMaterialProperty, "At least two B-H points are required.", "bhCurve",
            "CORE_MATERIAL_INVALID"));

    double previousH = -1.0;
    double previousB = -1.0;
    for (const auto& point : material.bhCurve) {
        const double h = point.fieldStrength.si();
        const double b = point.fluxDensity.si();
        if (!std::isfinite(h) || !std::isfinite(b) || h < 0.0 || b < 0.0 ||
            h <= previousH || b < previousB)
            return Result<void>::failure(materialError(
                ErrorCode::InvalidInput,
                "B-H points must be ordered by increasing H and non-decreasing B.",
                "bhCurve", "CORE_MATERIAL_INVALID"));
        previousH = h;
        previousB = b;
    }

    if (!material.lossData.empty() && material.lossDataReference.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData,
            "A separate provenance reference is required for core-loss data.",
            "lossDataReference", "CORE_MATERIAL_INVALID"));
    for (const auto& point : material.lossData) {
        if (!finitePositive(point.frequency.si()) ||
            !finitePositive(point.peakFluxDensity.si()) ||
            !finitePositive(point.temperature.si()) ||
            !std::isfinite(point.specificLoss.si()) || point.specificLoss.si() < 0.0)
            return Result<void>::failure(materialError(
                ErrorCode::InvalidInput,
                "Core-loss points require positive frequency, flux density and temperature, and non-negative loss.",
                "lossData", "CORE_MATERIAL_INVALID"));
    }
    return Result<void>::success();
}

Result<void> validate(const ConductorMaterial& material)
{
    if (material.id.empty() || material.grade.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData, "Conductor material ID and grade are required.",
            "material", "CONDUCTOR_MATERIAL_INVALID"));
    if (!validSource(material.dataSource))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput, "Unsupported conductor data source.", "dataSource",
            "CONDUCTOR_MATERIAL_INVALID"));
    if (material.dataSource == MaterialDataSource::ManufacturerDatasheet &&
        material.manufacturer.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData,
            "Manufacturer is required for manufacturer-datasheet conductor data.",
            "manufacturer", "CONDUCTOR_MATERIAL_INVALID"));
    if (material.sourceReference.empty() || material.datasetRevision.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData,
            "Conductor data provenance and dataset revision are required.",
            "provenance", "CONDUCTOR_MATERIAL_INVALID"));
    if (!finitePositive(material.resistivityAt20C.si()) ||
        !std::isfinite(material.temperatureCoefficient.si()) ||
        !finitePositive(material.density.si()))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput,
            "Conductor resistivity and density must be positive; temperature coefficient must be finite.",
            "physicalProperties", "CONDUCTOR_MATERIAL_INVALID"));
    return Result<void>::success();
}

Result<void> validate(const InsulationMaterial& material)
{
    if (material.id.empty() || material.grade.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData, "Insulation material ID and grade are required.",
            "material", "INSULATION_MATERIAL_INVALID"));
    if (!validSource(material.dataSource))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput, "Unsupported insulation data source.", "dataSource",
            "INSULATION_MATERIAL_INVALID"));
    if (material.dataSource == MaterialDataSource::ManufacturerDatasheet &&
        material.manufacturer.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData,
            "Manufacturer is required for manufacturer-datasheet insulation data.",
            "manufacturer", "INSULATION_MATERIAL_INVALID"));
    if (material.sourceReference.empty() || material.datasetRevision.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData,
            "Insulation data provenance and dataset revision are required.",
            "provenance", "INSULATION_MATERIAL_INVALID"));
    if (!finitePositive(material.maximumTemperature.si()) ||
        !finitePositive(material.dielectricStrength.si()))
        return Result<void>::failure(materialError(
            ErrorCode::MissingMaterialProperty,
            "Maximum temperature and dielectric strength must be positive.",
            "physicalProperties", "INSULATION_MATERIAL_INVALID"));
    if (material.thermalClass == InsulationThermalClass::NotSpecified)
        return Result<void>::failure(materialError(
            ErrorCode::MissingMaterialProperty, "Insulation thermal class is required.",
            "thermalClass", "INSULATION_MATERIAL_INVALID"));
    if (material.relativePermittivity &&
        (!std::isfinite(*material.relativePermittivity) || *material.relativePermittivity <= 0.0))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput, "Relative permittivity must be positive when supplied.",
            "relativePermittivity", "INSULATION_MATERIAL_INVALID"));
    if (material.density && !finitePositive(material.density->si()))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput, "Density must be positive when supplied.",
            "density", "INSULATION_MATERIAL_INVALID"));
    return Result<void>::success();
}

Result<void> CoreMaterialCatalog::add(CoreMaterial material)
{
    const auto checked = validate(material);
    if (!checked) return checked;
    return addUnique(materials_, std::move(material), "CORE_MATERIAL_INVALID", "Core material");
}
const CoreMaterial* CoreMaterialCatalog::find(const std::string& id) const noexcept
{
    const auto found = materials_.find(id);
    return found == materials_.end() ? nullptr : &found->second;
}

Result<void> ConductorMaterialCatalog::add(ConductorMaterial material)
{
    const auto checked = validate(material);
    if (!checked) return checked;
    return addUnique(materials_, std::move(material), "CONDUCTOR_MATERIAL_INVALID", "Conductor material");
}
const ConductorMaterial* ConductorMaterialCatalog::find(const std::string& id) const noexcept
{
    const auto found = materials_.find(id);
    return found == materials_.end() ? nullptr : &found->second;
}

Result<void> InsulationMaterialCatalog::add(InsulationMaterial material)
{
    const auto checked = validate(material);
    if (!checked) return checked;
    return addUnique(materials_, std::move(material), "INSULATION_MATERIAL_INVALID", "Insulation material");
}
const InsulationMaterial* InsulationMaterialCatalog::find(const std::string& id) const noexcept
{
    const auto found = materials_.find(id);
    return found == materials_.end() ? nullptr : &found->second;
}

} // namespace p135::domain
