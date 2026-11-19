#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace p135::filter {

enum class Connection { Wye, Delta };

struct NetworkDefinition {
    units::Voltage lineToLineVoltage;
    units::Frequency fundamentalFrequency;
    Connection connection{Connection::Wye};
};

struct SingleTunedBranchSpecification {
    std::uint32_t id{};
    std::string name;
    std::uint32_t tuningHarmonicOrder{};
    units::Power targetReactivePower;
    double qualityFactor{};
};

struct FilterSpecification {
    NetworkDefinition network;
    std::vector<SingleTunedBranchSpecification> branches;
};

struct SingleTunedBranchDesign {
    std::uint32_t id{};
    std::string name;
    std::uint32_t tuningHarmonicOrder{};
    units::Voltage phaseVoltage;
    units::Capacitance capacitance;
    units::Inductance inductance;
    units::Resistance dampingResistance;
    units::Power targetReactivePower;
    double qualityFactor{};
};

struct FilterDesign {
    NetworkDefinition network;
    std::vector<SingleTunedBranchDesign> branches;
};

/// Validates and analytically sizes three-phase, shunt, single-tuned series-RLC branches.
/// targetReactivePower is the total three-phase fundamental-frequency reactive power
/// of one branch. RMS line voltage and SI quantities are used throughout.
[[nodiscard]] Result<FilterDesign> synthesize(const FilterSpecification& specification);

} // namespace p135::filter
