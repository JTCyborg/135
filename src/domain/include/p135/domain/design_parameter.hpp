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

} // namespace p135::domain
