#include "p135/circuit/solver.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>

using namespace p135;
using namespace p135::circuit;

namespace {
const NodeId Gnd = 0;
const NodeId N1 = 1;
const NodeId N2 = 2;

bool near(double a, double b, double tolerance = 1.0e-8) {
    return std::abs(a - b) <= tolerance * std::max({1.0, std::abs(a), std::abs(b)});
}

void testDcResistiveDivider() {
    Circuit c;
    c.referenceNode = Gnd;
    c.nodes = {{Gnd, "0"}, {N1, "source"}, {N2, "output"}};
    c.elements = {
        Element::makeVoltageSource(1, N1, Gnd, {10.0, 0.0}, "V1"),
        Element::makeResistor(2, N1, N2, units::Resistance{1000.0}, "R1"),
        Element::makeResistor(3, N2, Gnd, units::Resistance{1000.0}, "R2")
    };

    Study s{c, AnalysisKind::DcOperatingPoint, units::Frequency{0.0}};
    const auto r = solve(s);
    assert(r);
    assert(near(r.value().nodeVoltages[2].rmsVoltage.real(), 5.0));
    assert(near(r.value().branches[1].rmsCurrent.real(), 0.005));
    assert(near(r.value().powerBalance.totalActivePowerW, 0.0));
    assert(r.value().solver.relativeResidual < 1.0e-12);
}

void testAcRcLowPass() {
    Circuit c;
    c.referenceNode = Gnd;
    c.nodes = {{Gnd, "0"}, {N1, "input"}, {N2, "output"}};
    c.elements = {
        Element::makeVoltageSource(1, N1, Gnd, {1.0, 0.0}, "Vin"),
        Element::makeResistor(2, N1, N2, units::Resistance{1000.0}, "R"),
        Element::makeCapacitor(3, N2, Gnd, units::Capacitance{1.0e-6}, "C")
    };

    Study s{c, AnalysisKind::AcSteadyState, units::Frequency{1000.0}};
    const auto r = solve(s);
    assert(r);
    const double expectedMagnitude = 1.0 / std::sqrt(1.0 + std::pow(2.0 * std::acos(-1.0), 2));
    const auto output = r.value().nodeVoltages[2].rmsVoltage;
    assert(near(std::abs(output), expectedMagnitude));
    assert(near(r.value().powerBalance.totalActivePowerW, 0.0));
    assert(near(r.value().powerBalance.totalReactivePowerVar, 0.0));
}

void testDcInductorShort() {
    Circuit c;
    c.referenceNode = Gnd;
    c.nodes = {{Gnd, "0"}, {N1, "supply"}, {N2, "afterL"}};
    c.elements = {
        Element::makeVoltageSource(1, N1, Gnd, {12.0, 0.0}),
        Element::makeInductor(2, N1, N2, units::Inductance{0.01}),
        Element::makeResistor(3, N2, Gnd, units::Resistance{6.0})
    };

    const auto r = solve(Study{c, AnalysisKind::DcOperatingPoint, units::Frequency{0.0}});
    assert(r);
    assert(near(r.value().nodeVoltages[2].rmsVoltage.real(), 12.0));
    assert(near(r.value().branches[1].rmsCurrent.real(), 2.0));
}

void testSingularCircuitIsRejected() {
    Circuit c;
    c.referenceNode = Gnd;
    c.nodes = {{Gnd, "0"}, {N1, "floating"}};
    c.elements = {Element::makeResistor(1, N1, Gnd, units::Resistance{1000.0})};
    // A floating node is not singular here: it has a valid path to reference.
    const auto valid = solve(Study{c, AnalysisKind::DcOperatingPoint, units::Frequency{0.0}});
    assert(valid);

    c.elements.clear();
    const auto invalid = solve(Study{c, AnalysisKind::DcOperatingPoint, units::Frequency{0.0}});
    assert(!invalid);
    assert(invalid.error().code == ErrorCode::SingularSystem);
}
}

int main() {
    testDcResistiveDivider();
    testAcRcLowPass();
    testDcInductorShort();
    testSingularCircuitIsRejected();
    return 0;
}
