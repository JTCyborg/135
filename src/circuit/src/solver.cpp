#include "p135/circuit/solver.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <unordered_map>
#include <utility>

namespace p135::circuit {
namespace {

using Matrix = std::vector<std::vector<Complex>>;
using Vector = std::vector<Complex>;

Error makeError(ErrorCode code, std::string message) {
    return Error{code, std::move(message), {}};
}

bool finite(Complex value) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

struct LinearSolution {
    Vector x;
    double minimumPivot{};
    double relativeResidual{};
};

Result<LinearSolution> solveLinear(Matrix a, Vector b, double tolerance,
                                   const CancellationToken& cancellation) {
    const std::size_t n = b.size();
    double minPivot = std::numeric_limits<double>::infinity();
    double matrixScale = 0.0;

    for (const auto& row : a)
        for (const auto value : row)
            matrixScale = std::max(matrixScale, std::abs(value));

    if (n == 0 || matrixScale == 0.0)
        return Result<LinearSolution>::failure(
            makeError(ErrorCode::SingularSystem, "MNA matrix is empty or zero."));

    for (std::size_t col = 0; col < n; ++col) {
        if (cancellation.isCancellationRequested())
            return Result<LinearSolution>::failure(
                makeError(ErrorCode::Cancelled, "Calculation cancelled."));

        std::size_t pivotRow = col;
        double pivotMagnitude = std::abs(a[col][col]);
        for (std::size_t row = col + 1; row < n; ++row) {
            const double candidate = std::abs(a[row][col]);
            if (candidate > pivotMagnitude) {
                pivotMagnitude = candidate;
                pivotRow = row;
            }
        }

        minPivot = std::min(minPivot, pivotMagnitude);
        if (pivotMagnitude <= tolerance * matrixScale)
            return Result<LinearSolution>::failure(
                makeError(ErrorCode::SingularSystem,
                          "MNA matrix is singular or ill-conditioned at the configured pivot tolerance."));

        if (pivotRow != col) {
            std::swap(a[pivotRow], a[col]);
            std::swap(b[pivotRow], b[col]);
        }

        const Complex pivot = a[col][col];
        for (std::size_t j = col; j < n; ++j)
            a[col][j] /= pivot;
        b[col] /= pivot;

        for (std::size_t row = 0; row < n; ++row) {
            if (row == col) continue;
            const Complex factor = a[row][col];
            if (std::abs(factor) == 0.0) continue;
            for (std::size_t j = col; j < n; ++j)
                a[row][j] -= factor * a[col][j];
            b[row] -= factor * b[col];
        }
    }

    // Residual is evaluated against the original system by the caller's assembly copy.
    return Result<LinearSolution>::success(
        LinearSolution{std::move(b), minPivot, 0.0});
}

} // namespace

Element Element::makeResistor(ComponentId id, NodeId p, NodeId n,
                              units::Resistance value, std::string name) {
    Element e; e.id=id; e.positive=p; e.negative=n; e.name=std::move(name);
    e.kind=ElementKind::Resistor; e.resistor={value}; return e;
}
Element Element::makeInductor(ComponentId id, NodeId p, NodeId n,
                              units::Inductance value, std::string name) {
    Element e; e.id=id; e.positive=p; e.negative=n; e.name=std::move(name);
    e.kind=ElementKind::Inductor; e.inductor={value}; return e;
}
Element Element::makeCapacitor(ComponentId id, NodeId p, NodeId n,
                               units::Capacitance value, std::string name) {
    Element e; e.id=id; e.positive=p; e.negative=n; e.name=std::move(name);
    e.kind=ElementKind::Capacitor; e.capacitor={value}; return e;
}
Element Element::makeVoltageSource(ComponentId id, NodeId p, NodeId n,
                                   Complex value, std::string name) {
    Element e; e.id=id; e.positive=p; e.negative=n; e.name=std::move(name);
    e.kind=ElementKind::VoltageSource; e.voltageSource={value}; return e;
}
Element Element::makeCurrentSource(ComponentId id, NodeId p, NodeId n,
                                   Complex value, std::string name) {
    Element e; e.id=id; e.positive=p; e.negative=n; e.name=std::move(name);
    e.kind=ElementKind::CurrentSource; e.currentSource={value}; return e;
}

Result<ElectricalResult> solve(const Study& study, const CancellationToken& cancellation) {
    const auto& circuit = study.circuit;
    if (!std::isfinite(study.pivotTolerance) || study.pivotTolerance <= 0.0)
        return Result<ElectricalResult>::failure(
            makeError(ErrorCode::InvalidInput, "pivotTolerance must be finite and positive."));
    if (!std::isfinite(study.frequency.si()) || study.frequency.si() < 0.0)
        return Result<ElectricalResult>::failure(
            makeError(ErrorCode::InvalidInput, "Frequency must be finite and non-negative."));
    if (study.analysis == AnalysisKind::DcOperatingPoint && study.frequency.si() != 0.0)
        return Result<ElectricalResult>::failure(
            makeError(ErrorCode::InvalidInput, "DC study frequency must be zero."));
    if (study.analysis == AnalysisKind::AcSteadyState && study.frequency.si() <= 0.0)
        return Result<ElectricalResult>::failure(
            makeError(ErrorCode::InvalidInput, "AC study frequency must be positive."));

    std::unordered_map<NodeId, std::size_t> nodeIndex;
    for (const auto& node : circuit.nodes) {
        if (!nodeIndex.emplace(node.id, nodeIndex.size()).second)
            return Result<ElectricalResult>::failure(
                makeError(ErrorCode::InvalidInput, "Duplicate node identifier."));
    }
    const auto refIt = nodeIndex.find(circuit.referenceNode);
    if (refIt == nodeIndex.end())
        return Result<ElectricalResult>::failure(
            makeError(ErrorCode::InvalidInput, "Reference node does not exist."));

    const std::size_t nodeCount = circuit.nodes.size() - 1;
    std::unordered_map<NodeId, std::size_t> unknownNode;
    std::size_t next = 0;
    for (const auto& node : circuit.nodes)
        if (node.id != circuit.referenceNode) unknownNode.emplace(node.id, next++);

    std::unordered_map<ComponentId, bool> componentIds;
    std::size_t voltageCount = 0;
    for (const auto& e : circuit.elements) {
        if (!componentIds.emplace(e.id, true).second)
            return Result<ElectricalResult>::failure(
                makeError(ErrorCode::InvalidInput, "Duplicate component identifier."));
        if (!nodeIndex.contains(e.positive) || !nodeIndex.contains(e.negative))
            return Result<ElectricalResult>::failure(
                makeError(ErrorCode::InvalidInput, "Element references an unknown node."));
        if (e.positive == e.negative)
            return Result<ElectricalResult>::failure(
                makeError(ErrorCode::InvalidInput, "Element terminals must be different."));
        if (e.kind == ElementKind::VoltageSource ||
            (e.kind == ElementKind::Inductor && study.analysis == AnalysisKind::DcOperatingPoint))
            ++voltageCount;
    }

    const std::size_t order = nodeCount + voltageCount;
    if (order == 0)
        return Result<ElectricalResult>::failure(
            makeError(ErrorCode::InvalidInput, "Circuit contains no unknowns."));
    Matrix matrix(order, Vector(order, Complex{}));
    Vector rhs(order, Complex{});
    Matrix original = matrix;
    Vector originalRhs = rhs;

    auto stampAdmittance = [&](NodeId p, NodeId n, Complex y) {
        const auto pi = unknownNode.find(p);
        const auto ni = unknownNode.find(n);
        if (pi != unknownNode.end()) matrix[pi->second][pi->second] += y;
        if (ni != unknownNode.end()) matrix[ni->second][ni->second] += y;
        if (pi != unknownNode.end() && ni != unknownNode.end()) {
            matrix[pi->second][ni->second] -= y;
            matrix[ni->second][pi->second] -= y;
        }
    };
    auto stampConstraint = [&](NodeId p, NodeId n, std::size_t branch, Complex voltage) {
        const std::size_t k = nodeCount + branch;
        const auto pi = unknownNode.find(p);
        const auto ni = unknownNode.find(n);
        if (pi != unknownNode.end()) {
            matrix[pi->second][k] += 1.0;
            matrix[k][pi->second] += 1.0;
        }
        if (ni != unknownNode.end()) {
            matrix[ni->second][k] -= 1.0;
            matrix[k][ni->second] -= 1.0;
        }
        rhs[k] += voltage;
    };

    std::unordered_map<ComponentId, std::size_t> sourceBranch;
    std::size_t branch = 0;
    for (const auto& e : circuit.elements) {
        if (e.kind == ElementKind::VoltageSource ||
            (e.kind == ElementKind::Inductor && study.analysis == AnalysisKind::DcOperatingPoint)) {
            sourceBranch.emplace(e.id, branch++);
        }
    }

    const double omega = 2.0 * std::acos(-1.0) * study.frequency.si();
    for (const auto& e : circuit.elements) {
        if (cancellation.isCancellationRequested())
            return Result<ElectricalResult>::failure(
                makeError(ErrorCode::Cancelled, "Calculation cancelled during matrix assembly."));

        switch (e.kind) {
        case ElementKind::Resistor: {
            const double r = e.resistor.resistance.si();
            if (!std::isfinite(r) || r <= 0.0)
                return Result<ElectricalResult>::failure(
                    makeError(ErrorCode::InvalidInput, "Resistance must be finite and positive."));
            stampAdmittance(e.positive, e.negative, 1.0 / r);
            break;
        }
        case ElementKind::Capacitor: {
            const double c = e.capacitor.capacitance.si();
            if (!std::isfinite(c) || c <= 0.0)
                return Result<ElectricalResult>::failure(
                    makeError(ErrorCode::InvalidInput, "Capacitance must be finite and positive."));
            if (study.analysis == AnalysisKind::AcSteadyState)
                stampAdmittance(e.positive, e.negative, Complex{0.0, omega * c});
            break;
        }
        case ElementKind::Inductor: {
            const double l = e.inductor.inductance.si();
            if (!std::isfinite(l) || l <= 0.0)
                return Result<ElectricalResult>::failure(
                    makeError(ErrorCode::InvalidInput, "Inductance must be finite and positive."));
            if (study.analysis == AnalysisKind::DcOperatingPoint)
                stampConstraint(e.positive, e.negative, sourceBranch.at(e.id), Complex{});
            else
                stampAdmittance(e.positive, e.negative, Complex{0.0, -1.0 / (omega * l)});
            break;
        }
        case ElementKind::VoltageSource:
            if (!finite(e.voltageSource.rmsVoltage))
                return Result<ElectricalResult>::failure(
                    makeError(ErrorCode::InvalidInput, "Voltage source value must be finite."));
            stampConstraint(e.positive, e.negative, sourceBranch.at(e.id),
                            e.voltageSource.rmsVoltage);
            break;
        case ElementKind::CurrentSource: {
            const auto current = e.currentSource.rmsCurrent;
            if (!finite(current))
                return Result<ElectricalResult>::failure(
                    makeError(ErrorCode::InvalidInput, "Current source value must be finite."));
            const auto pi = unknownNode.find(e.positive);
            const auto ni = unknownNode.find(e.negative);
            if (pi != unknownNode.end()) rhs[pi->second] -= current;
            if (ni != unknownNode.end()) rhs[ni->second] += current;
            break;
        }
        }
    }

    original = matrix;
    originalRhs = rhs;
    auto solved = solveLinear(std::move(matrix), std::move(rhs),
                              study.pivotTolerance, cancellation);
    if (!solved)
        return Result<ElectricalResult>::failure(solved.error());

    const auto& x = solved.value().x;
    ElectricalResult result;
    result.solver.matrixOrder = order;
    result.solver.minimumPivotMagnitude = solved.value().minimumPivot;

    auto voltageAt = [&](NodeId id) -> Complex {
        if (id == circuit.referenceNode) return {};
        return x[unknownNode.at(id)];
    };
    for (const auto& node : circuit.nodes)
        result.nodeVoltages.push_back({node.id, voltageAt(node.id)});

    Complex totalPower{};
    for (const auto& e : circuit.elements) {
        const Complex v = voltageAt(e.positive) - voltageAt(e.negative);
        Complex current{};
        switch (e.kind) {
        case ElementKind::Resistor:
            current = v / e.resistor.resistance.si();
            break;
        case ElementKind::Capacitor:
            if (study.analysis == AnalysisKind::AcSteadyState)
                current = Complex{0.0, omega * e.capacitor.capacitance.si()} * v;
            break;
        case ElementKind::Inductor:
            if (study.analysis == AnalysisKind::DcOperatingPoint)
                current = x[nodeCount + sourceBranch.at(e.id)];
            else
                current = v / Complex{0.0, omega * e.inductor.inductance.si()};
            break;
        case ElementKind::VoltageSource:
            current = x[nodeCount + sourceBranch.at(e.id)];
            break;
        case ElementKind::CurrentSource:
            current = e.currentSource.rmsCurrent;
            break;
        }
        const Complex power = v * std::conj(current);
        totalPower += power;
        result.branches.push_back({e.id, current, power});
    }

    result.powerBalance.totalActivePowerW = totalPower.real();
    result.powerBalance.totalReactivePowerVar = totalPower.imag();
    result.powerBalance.residualMagnitudeVA = std::abs(totalPower);

    // Relative residual ||Ax-b|| / (||A|| ||x|| + ||b||).
    double residual2 = 0.0, aNorm2 = 0.0, xNorm2 = 0.0, bNorm2 = 0.0;
    for (std::size_t i = 0; i < order; ++i) {
        Complex ax{};
        double rowNorm2 = 0.0;
        for (std::size_t j = 0; j < order; ++j) {
            ax += original[i][j] * x[j];
            rowNorm2 += std::norm(original[i][j]);
        }
        residual2 += std::norm(ax - originalRhs[i]);
        aNorm2 += rowNorm2;
        xNorm2 += std::norm(x[i]);
        bNorm2 += std::norm(originalRhs[i]);
    }
    const double denominator = std::sqrt(aNorm2) * std::sqrt(xNorm2) + std::sqrt(bNorm2);
    result.solver.relativeResidual = denominator > 0.0 ? std::sqrt(residual2) / denominator
                                                        : std::sqrt(residual2);

    return Result<ElectricalResult>::success(std::move(result));
}


} // namespace p135::circuit
