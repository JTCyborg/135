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

} // namespace p135::domain
