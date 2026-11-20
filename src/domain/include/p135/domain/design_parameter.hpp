#pragma once

#include "p135/core/result.hpp"

#include <optional>
#include <string>

namespace p135::domain {

enum class DesignControlMode { Auto, Fixed, Limit };
enum class DesignParameterStatus {
    NotCalculated,
    Calculated,
    InputChanged,
    NotVerified
};

// SI-valued scalar used by the project design layer. The calculated value is
// solver output; acceptedValue is the value consumed by downstream models.
// Limits are constraints, not values to be silently clamped after calculation.
struct DesignParameter {
    std::string id;
    std::string displayUnit;
    DesignControlMode mode{DesignControlMode::Auto};
    std::optional<double> calculatedValue;
    std::optional<double> acceptedValue;
    std::optional<double> minimumLimit;
    std::optional<double> maximumLimit;
    DesignParameterStatus status{DesignParameterStatus::NotCalculated};
};

// Validates control semantics only. It does not calculate a physical design,
// choose engineering limits, or certify the selected value.
[[nodiscard]] Result<void> validate(const DesignParameter& parameter);

// Applies an editor change without silently retaining a value from the old mode.
// FIXED requires an explicit accepted value; LIMIT requires at least one bound.
[[nodiscard]] Result<DesignParameter> editDesignParameter(
    const DesignParameter& current,
    DesignControlMode mode,
    std::optional<double> fixedValue,
    std::optional<double> minimumLimit,
    std::optional<double> maximumLimit);

// Applies a fresh solver candidate according to the selected control mode.
// AUTO accepts the candidate, FIXED preserves the user value, LIMIT accepts
// only candidates inside the explicit bounds. Failure leaves the input untouched.
[[nodiscard]] Result<DesignParameter> applyDesignParameterCalculation(
    const DesignParameter& parameter,
    double calculatedCandidate);

// Returns the value downstream stages must consume. It never substitutes a
// calculated value for a missing FIXED value or clamps a LIMIT result.
[[nodiscard]] Result<double> resolvedDesignParameterValue(
    const DesignParameter& parameter);

} // namespace p135::domain
