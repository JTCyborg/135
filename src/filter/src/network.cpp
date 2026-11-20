#include "p135/filter/network.hpp"

#include <cmath>
#include <array>
#include <complex>
#include <cstdint>
#include <string>
#include <utility>

namespace p135::filter {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
using circuit::Complex;

Error invalid(std::string message)
{
    return Error{ErrorCode::InvalidInput, std::move(message), {}};
}

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

Result<circuit::Circuit> buildCircuit(const FilterDesign& design,
                                      units::Resistance sourceResistance,
                                      units::Inductance sourceInductance)
{
    const double lineVoltage = design.network.lineToLineVoltage.si();
    const double frequency = design.network.fundamentalFrequency.si();
    const double sourceR = sourceResistance.si();
    const double sourceL = sourceInductance.si();
    if (!std::isfinite(sourceR) || sourceR < 0.0 ||
        !std::isfinite(sourceL) || sourceL < 0.0)
        return Result<circuit::Circuit>::failure(
            invalid("Thevenin source resistance and inductance must be finite and non-negative."));
    if (!finitePositive(lineVoltage) || !finitePositive(frequency))
        return Result<circuit::Circuit>::failure(
            invalid("Filter design must contain finite positive line voltage and frequency."));
    if (design.network.connection != Connection::Wye &&
        design.network.connection != Connection::Delta)
        return Result<circuit::Circuit>::failure(invalid("Unsupported filter connection."));
    if (design.branches.empty())
        return Result<circuit::Circuit>::failure(invalid("Filter design has no branches."));

    constexpr circuit::NodeId neutral = 0;
    constexpr circuit::NodeId phaseA = 1;
    constexpr circuit::NodeId phaseB = 2;
    constexpr circuit::NodeId phaseC = 3;

    circuit::Circuit result;
    result.referenceNode = neutral;
    result.nodes = {
        {neutral, "N"},
        {phaseA, "L1"},
        {phaseB, "L2"},
        {phaseC, "L3"}
    };

    const double phaseVoltage = lineVoltage / std::sqrt(3.0);
    const bool hasSourceImpedance = sourceR > 0.0 || sourceL > 0.0;
    const std::array<circuit::NodeId, 3> phaseNodes{phaseA, phaseB, phaseC};
    const std::array<circuit::NodeId, 3> sourceNodes{4, 5, 6};
    const std::array<double, 3> sourceAngles{0.0, -2.0 * kPi / 3.0, 2.0 * kPi / 3.0};
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const auto sourceNode = hasSourceImpedance ? sourceNodes[phase] : phaseNodes[phase];
        if (hasSourceImpedance)
            result.nodes.push_back({sourceNode, "Thevenin source L" + std::to_string(phase + 1)});
        result.elements.push_back(circuit::Element::makeVoltageSource(
            static_cast<circuit::ComponentId>(phase + 1), sourceNode, neutral,
            std::polar(phaseVoltage, sourceAngles[phase]),
            "Thevenin voltage source L" + std::to_string(phase + 1)));
    }

    circuit::ComponentId nextComponent = 4;
    circuit::NodeId nextNode = hasSourceImpedance ? 10 : 4;
    if (hasSourceImpedance) {
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const auto sourceNode = sourceNodes[phase];
            const auto busNode = phaseNodes[phase];
            const std::string suffix = " L" + std::to_string(phase + 1);
            if (sourceR > 0.0 && sourceL > 0.0) {
                const auto intermediateNode = static_cast<circuit::NodeId>(7 + phase);
                result.nodes.push_back({intermediateNode, "Thevenin R-L junction" + suffix});
                result.elements.push_back(circuit::Element::makeResistor(
                    nextComponent++, sourceNode, intermediateNode, sourceResistance,
                    "Thevenin resistance" + suffix));
                result.elements.push_back(circuit::Element::makeInductor(
                    nextComponent++, intermediateNode, busNode, sourceInductance,
                    "Thevenin inductance" + suffix));
            } else if (sourceR > 0.0) {
                result.elements.push_back(circuit::Element::makeResistor(
                    nextComponent++, sourceNode, busNode, sourceResistance,
                    "Thevenin resistance" + suffix));
            } else if (sourceL > 0.0) {
                result.elements.push_back(circuit::Element::makeInductor(
                    nextComponent++, sourceNode, busNode, sourceInductance,
                    "Thevenin inductance" + suffix));
            }
        }
    }

    const auto addSeriesBranch = [&](circuit::NodeId start, circuit::NodeId finish,
                                     const SingleTunedBranchDesign& branch,
                                     const std::string& suffix,
                                     circuit::Circuit& target,
                                     circuit::ComponentId& componentId,
                                     circuit::NodeId& nodeId) {
        const circuit::NodeId afterR = nodeId++;
        const circuit::NodeId afterL = nodeId++;
        target.nodes.push_back({afterR, branch.name + " " + suffix + " R-L"});
        target.nodes.push_back({afterL, branch.name + " " + suffix + " L-C"});
        target.elements.push_back(circuit::Element::makeResistor(
            componentId++, start, afterR, branch.dampingResistance,
            branch.name + " " + suffix + " R"));
        target.elements.push_back(circuit::Element::makeInductor(
            componentId++, afterR, afterL, branch.inductance,
            branch.name + " " + suffix + " L"));
        target.elements.push_back(circuit::Element::makeCapacitor(
            componentId++, afterL, finish, branch.capacitance,
            branch.name + " " + suffix + " C"));
    };

    for (const auto& branch : design.branches) {
        if (branch.id == 0 || !finitePositive(branch.capacitance.si()) ||
            !finitePositive(branch.inductance.si()) ||
            !finitePositive(branch.dampingResistance.si()))
            return Result<circuit::Circuit>::failure(
                invalid("Filter branch contains an invalid identifier or component value."));

        if (design.network.connection == Connection::Wye) {
            addSeriesBranch(phaseA, neutral, branch, "L1", result, nextComponent, nextNode);
            addSeriesBranch(phaseB, neutral, branch, "L2", result, nextComponent, nextNode);
            addSeriesBranch(phaseC, neutral, branch, "L3", result, nextComponent, nextNode);
        } else {
            addSeriesBranch(phaseA, phaseB, branch, "L1-L2", result, nextComponent, nextNode);
            addSeriesBranch(phaseB, phaseC, branch, "L2-L3", result, nextComponent, nextNode);
            addSeriesBranch(phaseC, phaseA, branch, "L3-L1", result, nextComponent, nextNode);
        }
    }

    return Result<circuit::Circuit>::success(std::move(result));
}

} // namespace p135::filter
