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

    DesignParameter invalidId;
    assert(!validate(invalidId));

    DesignParameter nonFinite{"winding.turns", "turn"};
    nonFinite.mode = DesignControlMode::Fixed;
    nonFinite.acceptedValue = std::numeric_limits<double>::infinity();
    assert(!validate(nonFinite));
    return 0;
}
