#include "p135/filter/synthesis.hpp"

#include <cassert>
#include <cmath>
#include <limits>

using namespace p135;
using namespace p135::filter;

namespace {

bool near(double actual, double expected, double relativeTolerance = 1.0e-9)
{
    return std::abs(actual - expected) <=
        relativeTolerance * (std::abs(expected) > 1.0 ? std::abs(expected) : 1.0);
}

FilterSpecification validWyeSpecification()
{
    FilterSpecification spec;
    spec.network.lineToLineVoltage = units::Voltage{400.0};
    spec.network.fundamentalFrequency = units::Frequency{50.0};
    spec.network.connection = Connection::Wye;
    spec.branches.push_back(SingleTunedBranchSpecification{
        1, "5th harmonic", 5, units::Power{100000.0}, 50.0
    });
    return spec;
}

} // namespace

int main()
{
    {
        const auto result = synthesize(validWyeSpecification());
        assert(result);
        assert(result.value().branches.size() == 1);
        const auto& branch = result.value().branches.front();
        assert(near(branch.phaseVoltage.si(), 400.0 / std::sqrt(3.0)));
        const double expectedC = (100000.0 / 3.0) * (1.0 - 1.0 / 25.0) /\n            (2.0 * 3.14159265358979323846 * 50.0 *\n             std::pow(400.0 / std::sqrt(3.0), 2.0));\n        assert(near(branch.capacitance.si(), expectedC));\n        const double expectedL = 1.0 / std::pow(2.0 * 3.14159265358979323846 * 250.0, 2.0) / expectedC;\n        assert(near(branch.inductance.si(), expectedL));\n        assert(near(branch.dampingResistance.si(),\n                    2.0 * 3.14159265358979323846 * 250.0 * expectedL / 50.0));
    }

    {
        auto spec = validWyeSpecification();
        spec.network.connection = Connection::Delta;
        const auto result = synthesize(spec);
        assert(result);
        assert(near(result.value().branches.front().phaseVoltage.si(), 400.0));
        assert(result.value().branches.front().capacitance.si() <
               synthesize(validWyeSpecification()).value().branches.front().capacitance.si());
    }

    {
        auto spec = validWyeSpecification();
        spec.branches.front().tuningHarmonicOrder = 1;
        assert(!synthesize(spec));
    }
    {
        auto spec = validWyeSpecification();
        spec.branches.front().targetReactivePower = units::Power{0.0};
        assert(!synthesize(spec));
    }
    {
        auto spec = validWyeSpecification();
        spec.branches.front().qualityFactor = std::numeric_limits<double>::quiet_NaN();
        assert(!synthesize(spec));
    }
    {
        auto spec = validWyeSpecification();
        spec.branches.push_back(spec.branches.front());
        assert(!synthesize(spec));
    }
    {
        auto spec = validWyeSpecification();
        spec.branches.clear();
        assert(!synthesize(spec));
    }

    return 0;
}
