#include "p135/domain/design_parameter.hpp"

#include <cmath>
#include <utility>

namespace p135::domain {
namespace {
Result<void> invalid(const std::string& message)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = message;
    return Result<void>::failure(std::move(error));
}

bool finite(const std::optional<double>& value)
{
    return !value || std::isfinite(*value);
}
}

Result<void> validate(const DesignParameter& parameter)
{
    if (parameter.id.empty())
        return invalid("Design parameter id must not be empty.");
    if (!finite(parameter.calculatedValue) || !finite(parameter.acceptedValue)
        || !finite(parameter.minimumLimit) || !finite(parameter.maximumLimit))
        return invalid("Design parameter values and limits must be finite.");

    if (parameter.minimumLimit && parameter.maximumLimit
        && *parameter.minimumLimit > *parameter.maximumLimit)
        return invalid("Design parameter minimum limit exceeds its maximum limit.");

    if (parameter.acceptedValue) {
        if (parameter.minimumLimit && *parameter.acceptedValue < *parameter.minimumLimit)
            return invalid("Accepted design value is below its minimum limit.");
        if (parameter.maximumLimit && *parameter.acceptedValue > *parameter.maximumLimit)
            return invalid("Accepted design value exceeds its maximum limit.");
    }

    switch (parameter.mode) {
    case DesignControlMode::Auto:
        if (parameter.acceptedValue && !parameter.calculatedValue)
            return invalid("AUTO mode cannot accept a user value before a calculated value exists.");
        if (parameter.acceptedValue && parameter.calculatedValue
            && *parameter.acceptedValue != *parameter.calculatedValue)
            return invalid("AUTO mode accepted value must match the calculated value.");
        if (parameter.minimumLimit || parameter.maximumLimit)
            return invalid("AUTO mode cannot carry user limits; use LIMIT mode.");
        break;
    case DesignControlMode::Fixed:
        if (!parameter.acceptedValue)
            return invalid("FIXED mode requires an accepted user value.");
        break;
    case DesignControlMode::Limit:
        if (!parameter.minimumLimit && !parameter.maximumLimit)
            return invalid("LIMIT mode requires at least one user-supplied bound.");
        break;
    }
    return Result<void>::success();
}

Result<DesignParameter> editDesignParameter(
    const DesignParameter& current,
    DesignControlMode mode,
    std::optional<double> fixedValue,
    std::optional<double> minimumLimit,
    std::optional<double> maximumLimit)
{
    DesignParameter edited = current;
    edited.mode = mode;
    edited.minimumLimit = minimumLimit;
    edited.maximumLimit = maximumLimit;
    edited.status = DesignParameterStatus::InputChanged;

    switch (mode) {
    case DesignControlMode::Auto:
        edited.acceptedValue.reset();
        edited.minimumLimit.reset();
        edited.maximumLimit.reset();
        break;
    case DesignControlMode::Fixed:
        edited.acceptedValue = fixedValue;
        break;
    case DesignControlMode::Limit:
        edited.acceptedValue.reset();
        break;
    }

    const auto valid = validate(edited);
    if (!valid)
        return Result<DesignParameter>::failure(valid.error());
    return Result<DesignParameter>::success(std::move(edited));
}

Result<DesignParameter> applyDesignParameterCalculation(
    const DesignParameter& parameter,
    double calculatedCandidate)
{
    if (!std::isfinite(calculatedCandidate))
        return Result<DesignParameter>::failure(
            {ErrorCode::InvalidInput, "Solver candidate must be finite.", {}});

    DesignParameter updated = parameter;
    updated.calculatedValue = calculatedCandidate;
    switch (updated.mode) {
    case DesignControlMode::Auto:
        updated.acceptedValue = calculatedCandidate;
        break;
    case DesignControlMode::Fixed:
        if (!updated.acceptedValue)
            return Result<DesignParameter>::failure(
                {ErrorCode::InvalidInput, "FIXED parameter has no user-supplied value.", {}});
        break;
    case DesignControlMode::Limit:
        if (!updated.minimumLimit && !updated.maximumLimit)
            return Result<DesignParameter>::failure(
                {ErrorCode::InvalidInput, "LIMIT parameter has no search bounds.", {}});
        if ((updated.minimumLimit && calculatedCandidate < *updated.minimumLimit) ||
            (updated.maximumLimit && calculatedCandidate > *updated.maximumLimit))
            return Result<DesignParameter>::failure(
                {ErrorCode::InvalidInput,
                 "Solver candidate is outside the user-specified LIMIT bounds.", {}});
        updated.acceptedValue = calculatedCandidate;
        break;
    }

    updated.status = DesignParameterStatus::Calculated;
    const auto valid = validate(updated);
    if (!valid)
        return Result<DesignParameter>::failure(valid.error());
    return Result<DesignParameter>::success(std::move(updated));
}

Result<double> resolvedDesignParameterValue(const DesignParameter& parameter)
{
    const auto valid = validate(parameter);
    if (!valid)
        return Result<double>::failure(valid.error());

    switch (parameter.mode) {
    case DesignControlMode::Auto:
        if (!parameter.calculatedValue)
            return Result<double>::failure(
                {ErrorCode::InvalidInput, "AUTO parameter has not been calculated.", {}});
        return Result<double>::success(*parameter.calculatedValue);
    case DesignControlMode::Fixed:
        return Result<double>::success(*parameter.acceptedValue);
    case DesignControlMode::Limit:
        if (!parameter.acceptedValue)
            return Result<double>::failure(
                {ErrorCode::InvalidInput, "LIMIT parameter has no feasible calculated value.", {}});
        return Result<double>::success(*parameter.acceptedValue);
    }
    return Result<double>::failure(
        {ErrorCode::InternalError, "Unknown design parameter control mode.", {}});
}

} // namespace p135::domain
