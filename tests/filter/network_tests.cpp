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

void testTheveninSourceImpedanceIsPerPhaseSeriesRL()
{
    const auto design = makeDesign(Connection::Wye);
    const auto circuitResult = buildCircuit(
        design, units::Resistance{0.0016}, units::Inductance{30.5e-6});
    assert(circuitResult);
    const auto& circuit = circuitResult.value();
    assert(circuit.nodes.size() == 16);
    assert(circuit.elements.size() == 18);

    const auto solved = solve(Study{
        circuit, AnalysisKind::AcSteadyState, units::Frequency{50.0}
    });
    assert(solved);
    const auto findCurrent = [&solved](ComponentId id) {
        const auto it = std::find_if(solved.value().branches.begin(),
            solved.value().branches.end(),
            [id](const BranchResult& branch) { return branch.component == id; });
        assert(it != solved.value().branches.end());
        return it->rmsCurrent;
    };
    // For phase A, voltage source -> R -> L -> filter bus is a true series path.
    assert(std::abs(findCurrent(1) + findCurrent(4)) < 1.0e-8);
    assert(std::abs(findCurrent(4) - findCurrent(5)) < 1.0e-8);
    assert(std::abs(findCurrent(1)) > 0.0);
    assert(solved.value().solver.relativeResidual < 1.0e-10);

    const auto ideal = buildCircuit(design);
    assert(ideal);
    const auto idealSolved = solve(Study{
        ideal.value(), AnalysisKind::AcSteadyState, units::Frequency{50.0}
    });
    assert(idealSolved);
    const auto busVoltage = [&solved](NodeId id) {
        const auto it = std::find_if(solved.value().nodeVoltages.begin(),
            solved.value().nodeVoltages.end(),
            [id](const NodeVoltage& node) { return node.node == id; });
        assert(it != solved.value().nodeVoltages.end());
        return std::abs(it->rmsVoltage);
    };
    const auto idealBusVoltage = [&idealSolved](NodeId id) {
        const auto it = std::find_if(idealSolved.value().nodeVoltages.begin(),
            idealSolved.value().nodeVoltages.end(),
            [id](const NodeVoltage& node) { return node.node == id; });
        assert(it != idealSolved.value().nodeVoltages.end());
        return std::abs(it->rmsVoltage);
    };
    assert(std::abs(busVoltage(1) - idealBusVoltage(1)) > 1.0e-5);
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
    testTheveninSourceImpedanceIsPerPhaseSeriesRL();
    testBuilderRejectsEmptyDesign();
    return 0;
}
