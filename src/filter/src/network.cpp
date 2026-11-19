#include "p135/filter/network.hpp"

#include <cmath>
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

Result<circuit::Circuit> buildCircuit(const FilterDesign& design)
{
    const double lineVoltage = design.network.lineToLineVoltage.si();
    const double frequency = design.network.fundamentalFrequency.si();
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
    result.elements.push_back(circuit::Element::makeVoltageSource(
        1, phaseA, neutral, std::polar(phaseVoltage, 0.0), "Source L1"));
    result.elements.push_back(circuit::Element::makeVoltageSource(
        2, phaseB, neutral, std::polar(phaseVoltage, -2.0 * kPi / 3.0), "Source L2"));
    result.elements.push_back(circuit::Element::makeVoltageSource(
        3, phaseC, neutral, std::polar(phaseVoltage, 2.0 * kPi / 3.0), "Source L3"));

    circuit::ComponentId nextComponent = 4;
    circuit::NodeId nextNode = 4;

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
