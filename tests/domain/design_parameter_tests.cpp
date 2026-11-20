#include "p135/domain/design_parameter.hpp"

#include <cassert>
#include <limits>

using namespace p135::domain;

int main()
{
    DesignParameter automatic{"core.limb.width", "m"};
    assert(validate(automatic));

    automatic.calculatedValue = 0.042;
    automatic.acceptedValue = 0.042;
    automatic.status = DesignParameterStatus::Calculated;
    assert(validate(automatic));

    automatic.acceptedValue = 0.043;
    assert(!validate(automatic));
    automatic.acceptedValue.reset();

    automatic.mode = DesignControlMode::Fixed;
    automatic.acceptedValue = 0.045;
    assert(validate(automatic));
    automatic.acceptedValue.reset();
    assert(!validate(automatic));

    DesignParameter limited{"core.stack.depth", "m"};
    limited.mode = DesignControlMode::Limit;
    limited.minimumLimit = 0.08;
    limited.maximumLimit = 0.12;
    assert(validate(limited));
    limited.acceptedValue = 0.10;
    assert(validate(limited));
    limited.acceptedValue = 0.13;
    assert(!validate(limited));
    limited.acceptedValue.reset();
    limited.minimumLimit = 0.13;
    assert(!validate(limited));

    // AUTO consumes the fresh solver result.
    DesignParameter editable{"winding.turns", "turn"};
    auto autoEdit = editDesignParameter(
        editable, DesignControlMode::Auto, std::nullopt, std::nullopt, std::nullopt);
    assert(autoEdit);
    auto autoCalculated = applyDesignParameterCalculation(autoEdit.value(), 120.0);
    assert(autoCalculated);
    assert(autoCalculated.value().acceptedValue == 120.0);
    assert(resolvedDesignParameterValue(autoCalculated.value()).value() == 120.0);

    // FIXED preserves the entered value even when the solver proposes another.
    auto fixedEdit = editDesignParameter(
        editable, DesignControlMode::Fixed, 96.0, std::nullopt, std::nullopt);
    assert(fixedEdit);
    auto fixedCalculated = applyDesignParameterCalculation(fixedEdit.value(), 120.0);
    assert(fixedCalculated);
    assert(fixedCalculated.value().calculatedValue == 120.0);
    assert(fixedCalculated.value().acceptedValue == 96.0);
    assert(resolvedDesignParameterValue(fixedCalculated.value()).value() == 96.0);

    // LIMIT accepts only a solver candidate inside the declared interval.
    auto limitEdit = editDesignParameter(
        editable, DesignControlMode::Limit, std::nullopt, 90.0, 130.0);
    assert(limitEdit);
    auto limitCalculated = applyDesignParameterCalculation(limitEdit.value(), 120.0);
    assert(limitCalculated && limitCalculated.value().acceptedValue == 120.0);
    assert(!applyDesignParameterCalculation(limitEdit.value(), 140.0));
    assert(!resolvedDesignParameterValue(limitEdit.value()));

    assert(!editDesignParameter(
        editable, DesignControlMode::Fixed, std::nullopt, std::nullopt, std::nullopt));
    assert(!editDesignParameter(
        editable, DesignControlMode::Limit, std::nullopt, std::nullopt, std::nullopt));
    assert(!applyDesignParameterCalculation(autoEdit.value(),
        std::numeric_limits<double>::quiet_NaN()));

    DesignParameter invalidId;
    assert(!validate(invalidId));

    DesignParameter nonFinite{"winding.turns", "turn"};
    nonFinite.mode = DesignControlMode::Fixed;
    nonFinite.acceptedValue = std::numeric_limits<double>::infinity();
    assert(!validate(nonFinite));
    return 0;
}
