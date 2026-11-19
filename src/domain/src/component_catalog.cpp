#include "p135/domain/component_catalog.hpp"

#include <cmath>
#include <utility>

namespace p135::domain {
namespace {
Error capacitorError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CAPACITOR_COMPONENT_INVALID", error.message,
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
} // namespace

Result<void> validate(const CapacitorComponent& capacitor)
{
    if (capacitor.id.empty())
        return Result<void>::failure(capacitorError(
            ErrorCode::MissingRequiredData, "Capacitor ID is required.", "id"));
    if (!validSource(capacitor.dataSource))
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput, "Unsupported capacitor data source.", "dataSource"));
    if (capacitor.dataSource == MaterialDataSource::ManufacturerDatasheet &&
        capacitor.manufacturer.empty())
        return Result<void>::failure(capacitorError(
            ErrorCode::MissingRequiredData,
            "Manufacturer is required for manufacturer-datasheet capacitor data.",
            "manufacturer"));
    if (capacitor.sourceReference.empty() || capacitor.datasetRevision.empty())
        return Result<void>::failure(capacitorError(
            ErrorCode::MissingRequiredData,
            "Capacitor source reference and dataset revision are required.", "provenance"));
    if (!finitePositive(capacitor.capacitance.si()))
        return Result<void>::failure(capacitorError(
            ErrorCode::MissingMaterialProperty, "Capacitance must be positive.", "capacitance"));
    if (!finitePositive(capacitor.ratedVoltage.si()))
        return Result<void>::failure(capacitorError(
            ErrorCode::MissingMaterialProperty, "Rated voltage must be positive.", "ratedVoltage"));
    if (!std::isfinite(capacitor.toleranceFraction) ||
        capacitor.toleranceFraction < 0.0 || capacitor.toleranceFraction >= 1.0)
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput, "Tolerance must be finite and in the range [0, 1).",
            "toleranceFraction"));
    if (!std::isfinite(capacitor.minimumTemperature.si()) ||
        !std::isfinite(capacitor.maximumTemperature.si()) ||
        capacitor.minimumTemperature.si() <= 0.0 ||
        capacitor.maximumTemperature.si() <= capacitor.minimumTemperature.si())
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput,
            "Capacitor temperature range must be positive and maximum must exceed minimum.",
            "temperatureRange"));
    if (capacitor.esr && (!std::isfinite(capacitor.esr->si()) || capacitor.esr->si() < 0.0))
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput, "ESR must be finite and non-negative when supplied.", "esr"));
    if (capacitor.esl && (!std::isfinite(capacitor.esl->si()) || capacitor.esl->si() < 0.0))
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput, "ESL must be finite and non-negative when supplied.", "esl"));
    if (capacitor.dissipationFactor &&
        (!std::isfinite(*capacitor.dissipationFactor) || *capacitor.dissipationFactor < 0.0))
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput,
            "Dissipation factor must be finite and non-negative when supplied.",
            "dissipationFactor"));
    if (capacitor.rippleCurrent &&
        (!std::isfinite(capacitor.rippleCurrent->si()) || capacitor.rippleCurrent->si() < 0.0))
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput,
            "Ripple current must be finite and non-negative when supplied.", "rippleCurrent"));
    if (capacitor.ratedLife && !finitePositive(capacitor.ratedLife->si()))
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput, "Rated life must be positive when supplied.", "ratedLife"));
    return Result<void>::success();
}

Result<void> CapacitorCatalog::add(CapacitorComponent capacitor)
{
    const auto checked = validate(capacitor);
    if (!checked) return checked;
    if (capacitors_.contains(capacitor.id))
        return Result<void>::failure(capacitorError(
            ErrorCode::InvalidInput, "Capacitor with this ID already exists.", "id"));
    const auto id = capacitor.id;
    capacitors_.emplace(id, std::move(capacitor));
    return Result<void>::success();
}

const CapacitorComponent* CapacitorCatalog::find(const std::string& id) const noexcept
{
    const auto found = capacitors_.find(id);
    return found == capacitors_.end() ? nullptr : &found->second;
}

} // namespace p135::domain
