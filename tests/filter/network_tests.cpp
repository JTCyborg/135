#include "p135/circuit/solver.hpp"
#include "p135/filter/network.hpp"
#include "p135/filter/synthesis.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

using namespace p135;
using namespace p135::filter;
using namespace p135::circuit;

namespace {

bool near(double actual, double expected, double relativeTolerance = 1.0e-6)
{
    return std::abs(actual - expected) <= relativeTolerance *
        std::max({1.0, std::abs(actual), std::abs(expected)});
}

FilterDesign makeDesign(Connection connection)
{
    FilterSpecification specification;
    specification.network.lineToLineVoltage = units::Voltage{400.0};
    specification.network.fundamentalFrequency = units::Frequency{50.0};
    specification.network.connection = connection;
    specification.branches.push_back({
        1, "5th harmonic", 5, units::Power{100000.0}, 50.0
    });
    const auto result = synthesize(specification);
    assert(result);
    return result.value();
}

Complex branchBankPower(const ElectricalResult& result)
{
    Complex total{};
    // First three entries are ideal source branches; the remaining entries
    // are passive R/L/C components of the filter.
    for (std::size_t i = 3; i < result.branches.size(); ++i)
        total += result.branches[i].complexPowerVA;
    return total;
}

void testWyeCircuitTopologyAndFundamentalPower()
{
    const auto circuitResult = buildCircuit(makeDesign(Connection::Wye));
    assert(circuitResult);
    const auto& circuit = circuitResult.value();

    assert(circuit.nodes.size() == 10); // N, L1-L3, plus two internal nodes per phase
    assert(circuit.elements.size() == 12); // 3 sources + 3 x (R, L, C)

    const auto solved = solve(Study{
        circuit, AnalysisKind::AcSteadyState, units::Frequency{50.0}
    });
    assert(solved);
    const auto power = branchBankPower(solved.value());
    assert(power.real() > 0.0); // damping resistor dissipates real power
    assert(near(power.imag(), -100000.0));
    assert(solved.value().solver.relativeResidual < 1.0e-10);
}

void testWyeCircuitAtTuningFrequency()
{
    const auto circuitResult = buildCircuit(makeDesign(Connection::Wye));
    assert(circuitResult);
    const auto solved = solve(Study{
        circuitResult.value(), AnalysisKind::AcSteadyState, units::Frequency{250.0}
    });
    assert(solved);
    const auto power = branchBankPower(solved.value());
    assert(power.real() > 0.0);
    assert(near(power.imag(), 0.0, 1.0e-5));
}

void testDeltaCircuitTopologyAndFundamentalPower()
{
    const auto circuitResult = buildCircuit(makeDesign(Connection::Delta));
    assert(circuitResult);
    const auto& circuit = circuitResult.value();

    assert(circuit.nodes.size() == 10);
    assert(circuit.elements.size() == 12);

    const auto solved = solve(Study{
        circuit, AnalysisKind::AcSteadyState, units::Frequency{50.0}
    });
    assert(solved);
    const auto power = branchBankPower(solved.value());
    assert(power.real() > 0.0); // damping resistor dissipates real power
    assert(near(power.imag(), -100000.0));
    assert(solved.value().solver.relativeResidual < 1.0e-10);
}

void testBuilderRejectsEmptyDesign()
{
    FilterDesign design;
    design.network.lineToLineVoltage = units::Voltage{400.0};
    design.network.fundamentalFrequency = units::Frequency{50.0};
    assert(!buildCircuit(design));
}

} // namespace

int main()
{
    testWyeCircuitTopologyAndFundamentalPower();
    testWyeCircuitAtTuningFrequency();
    testDeltaCircuitTopologyAndFundamentalPower();
    testBuilderRejectsEmptyDesign();
    return 0;
}
