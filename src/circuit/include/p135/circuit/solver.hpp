#pragma once

#include "p135/core/cancellation.hpp"
#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace p135::circuit {

using Complex = std::complex<double>;
using NodeId = std::uint32_t;
using ComponentId = std::uint32_t;

struct Node {
    NodeId id{};
    std::string name;
};

struct Resistor {
    units::Resistance resistance;
};
struct Inductor {
    units::Inductance inductance;
};
struct Capacitor {
    units::Capacitance capacitance;
};
struct VoltageSource {
    Complex rmsVoltage{};
};
struct CurrentSource {
    Complex rmsCurrent{};
};

enum class ElementKind { Resistor, Inductor, Capacitor, VoltageSource, CurrentSource };

struct Element {
    ComponentId id{};
    NodeId positive{};
    NodeId negative{};
    std::string name;
    Resistor resistor{};
    Inductor inductor{};
    Capacitor capacitor{};
    VoltageSource voltageSource{};
    CurrentSource currentSource{};
    ElementKind kind{ElementKind::Resistor};

    static Element makeResistor(ComponentId id, NodeId p, NodeId n,
                                units::Resistance value, std::string name = {});
    static Element makeInductor(ComponentId id, NodeId p, NodeId n,
                                units::Inductance value, std::string name = {});
    static Element makeCapacitor(ComponentId id, NodeId p, NodeId n,
                                 units::Capacitance value, std::string name = {});
    static Element makeVoltageSource(ComponentId id, NodeId p, NodeId n,
                                     Complex rmsVoltage, std::string name = {});
    static Element makeCurrentSource(ComponentId id, NodeId p, NodeId n,
                                     Complex rmsCurrent, std::string name = {});
};

struct Circuit {
    std::vector<Node> nodes;
    std::vector<Element> elements;
    NodeId referenceNode{};
};

enum class AnalysisKind { DcOperatingPoint, AcSteadyState };

struct Study {
    Circuit circuit;
    AnalysisKind analysis{AnalysisKind::DcOperatingPoint};
    units::Frequency frequency{units::Frequency{0.0}};
    double pivotTolerance{1.0e-12};
};

struct NodeVoltage {
    NodeId node{};
    Complex rmsVoltage{};
};

struct BranchResult {
    ComponentId component{};
    Complex rmsCurrent{};
    Complex complexPowerVA{};
};

struct PowerBalance {
    double totalActivePowerW{};
    double totalReactivePowerVar{};
    double residualMagnitudeVA{};
};

struct SolverDiagnostics {
    std::size_t matrixOrder{};
    double minimumPivotMagnitude{};
    double relativeResidual{};
};

struct ElectricalResult {
    std::vector<NodeVoltage> nodeVoltages;
    std::vector<BranchResult> branches;
    PowerBalance powerBalance;
    SolverDiagnostics solver;
};

Result<ElectricalResult> solve(const Study& study,
                               const CancellationToken& cancellation = {});

} // namespace p135::circuit
