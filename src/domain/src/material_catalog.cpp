#include "p135/domain/material_catalog.hpp"

#include <cmath>
#include <utility>

namespace p135::domain {
namespace {
Error materialError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CORE_MATERIAL_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}
bool finitePositive(double value) { return std::isfinite(value) && value > 0.0; }
} // namespace

Result<void> validate(const CoreMaterial& material)
{
    if (material.id.empty() || material.grade.empty() || material.manufacturer.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData, "Material ID, manufacturer and grade are required.", "material"));
    if (!finitePositive(material.density.si()))
        return Result<void>::failure(materialError(
            ErrorCode::MissingMaterialProperty, "Material density must be positive.", "density"));
    if (material.datasetRevision.empty() || material.sourceReference.empty())
        return Result<void>::failure(materialError(
            ErrorCode::MissingRequiredData, "Data revision and provenance reference are required.", "provenance"));
    if (material.bhCurve.size() < 2)
        return Result<void>::failure(materialError(
            ErrorCode::MissingMaterialProperty, "At least two B-H points are required.", "bhCurve"));

    double previousH = -1.0;
    double previousB = -1.0;
    for (const auto& point : material.bhCurve) {
        const double h = point.fieldStrength.si();
        const double b = point.fluxDensity.si();
        if (!std::isfinite(h) || !std::isfinite(b) || h < 0.0 || b < 0.0 ||
            h <= previousH || b < previousB)
            return Result<void>::failure(materialError(
                ErrorCode::InvalidInput, "B-H points must be ordered by increasing H and non-decreasing B.", "bhCurve"));
        previousH = h;
        previousB = b;
    }

    for (const auto& point : material.lossData) {
        if (!finitePositive(point.frequency.si()) ||
            !finitePositive(point.peakFluxDensity.si()) ||
            !finitePositive(point.temperature.si()) ||
            !std::isfinite(point.specificLoss.si()) || point.specificLoss.si() < 0.0)
            return Result<void>::failure(materialError(
                ErrorCode::InvalidInput,
                "Core-loss points require positive frequency, flux density and temperature, and non-negative loss.",
                "lossData"));
    }
    return Result<void>::success();
}

Result<void> CoreMaterialCatalog::add(CoreMaterial material)
{
    const auto checked = validate(material);
    if (!checked) return checked;
    if (materials_.contains(material.id))
        return Result<void>::failure(materialError(
            ErrorCode::InvalidInput, "A material with this ID already exists.", "id"));
    materials_.emplace(material.id, std::move(material));
    return Result<void>::success();
}

const CoreMaterial* CoreMaterialCatalog::find(const std::string& id) const noexcept
{
    const auto found = materials_.find(id);
    return found == materials_.end() ? nullptr : &found->second;
}

} // namespace p135::domain
