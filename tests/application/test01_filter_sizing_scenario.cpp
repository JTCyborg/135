#include "p135/application/ideal_reactor_sizing.hpp"
#include "p135/filter/synthesis.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

using namespace p135;
using namespace p135::application;
using namespace p135::filter;

namespace {
bool near(double actual, double expected, double relativeTolerance = 2.0e-4)
{
    return std::abs(actual - expected) <=
        relativeTolerance * std::max(std::abs(expected), 1.0e-12);
}

FilterSpecification test01()
{
    FilterSpecification specification;
    specification.network = {
        units::Voltage{400.0},
        units::Frequency{50.0},
        Connection::Wye
    };
    specification.branches.push_back({
        1, "TEST-01 / 5th harmonic", 5.0, units::Power{100000.0}, 50.0
    });
    return specification;
}
}

int main()
{
    // Electrical regression: 400 V line-to-line, 50 Hz, 100 kVAr,
    // Wye, 5th harmonic (250 Hz), quality factor 50.
    const auto design = synthesize(test01());
    assert(design);
    assert(design.value().branches.size() == 1);
    const auto& branch = design.value().branches.front();

    assert(near(branch.phaseVoltage.si(), 230.9401076758503));
    assert(near(branch.capacitance.si(), 1.910e-3));
    assert(near(branch.inductance.si(), 0.2122e-3));
    assert(near(branch.dampingResistance.si(), 0.006667));

    const double phaseRmsCurrent = 100000.0 /
        (std::sqrt(3.0) * 400.0);
    const double phasePeakCurrent = phaseRmsCurrent * std::sqrt(2.0);
    assert(near(phaseRmsCurrent, 144.3375672974064));

    // The following are deliberately explicit TEST FIXTURE assumptions.
    // They are not sourced material/design limits and must never be used to
    // release a physical design or generate CAD geometry.
    const IdealReactorSizingRequest sizing{
        branch.inductance.si(),
        phasePeakCurrent,
        phaseRmsCurrent,
        1.2,       // fixture Bpk limit, T
        3.0e6,     // fixture current density, A/m^2
        0.35,      // fixture window utilization
        0.95,      // fixture stacking factor
        2.0        // fixture gross Aw/Ae ratio
    };
    const auto preliminary = calculateIdealReactorAreaProduct(sizing);
    assert(preliminary);
    assert(preliminary.value().idealMinimumTurns >= 1);
    assert(preliminary.value().minimumEffectiveAreaProductM4 > 0.0);
    assert(preliminary.value().estimatedEffectiveCoreAreaM2 > 0.0);
    assert(preliminary.value().estimatedWindowAreaM2 > 0.0);

    const double achievedPeakFlux = branch.inductance.si() * phasePeakCurrent /
        (static_cast<double>(preliminary.value().idealMinimumTurns) *
         preliminary.value().estimatedEffectiveCoreAreaM2);
    assert(achievedPeakFlux <= sizing.allowablePeakFluxDensityT * (1.0 + 1.0e-12));

    // No material B-H/loss evidence, winding thermal data, or released
    // mechanical dimensions are supplied by this scenario. Consequently the
    // preliminary estimate is not a verified construction and cannot release CAD.
    assert(!near(preliminary.value().estimatedEffectiveCoreAreaM2, 0.0));
    return 0;
}
