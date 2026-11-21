#include "p135/application/filter_excitation_stage_adapter.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace p135::application {
namespace {
Error invalidArtifact(const char* message)
{
    return {ErrorCode::InvalidInput, message, {}};
}

bool positiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}
}

Result<std::string> makeFilterExcitationStageArtifact(const filter::FilterDesign& design)
{
    if (!positiveFinite(design.network.lineToLineVoltage.si()) ||
        !positiveFinite(design.network.fundamentalFrequency.si()) ||
        design.branches.empty())
        return Result<std::string>::failure(invalidArtifact(
            "Filter excitation requires positive finite network voltage/frequency and at least one branch."));

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    output << "schema=project135.filter-excitation.v1\n";
    output << "line_to_line_voltage_V=" << design.network.lineToLineVoltage.si() << '\n';
    output << "fundamental_frequency_Hz=" << design.network.fundamentalFrequency.si() << '\n';
    output << "connection=" << (design.network.connection == filter::Connection::Wye ? "WYE" : "DELTA") << '\n';
    output << "branch_count=" << design.branches.size() << '\n';

    for (std::size_t index = 0; index < design.branches.size(); ++index) {
        const auto& branch = design.branches[index];
        if (branch.id == 0 || !positiveFinite(branch.tuningHarmonicOrder) ||
            !positiveFinite(branch.phaseVoltage.si()) ||
            !positiveFinite(branch.capacitance.si()) ||
            !positiveFinite(branch.inductance.si()) ||
            !positiveFinite(branch.dampingResistance.si()) ||
            !positiveFinite(branch.targetReactivePower.si()) ||
            !positiveFinite(branch.qualityFactor))
            return Result<std::string>::failure(invalidArtifact(
                "Filter excitation contains a branch with an invalid ID or non-positive/non-finite electrical value."));

        output << "branch." << index << ".id=" << branch.id << '\n';
        output << "branch." << index << ".tuning_order=" << branch.tuningHarmonicOrder << '\n';
        output << "branch." << index << ".phase_voltage_V=" << branch.phaseVoltage.si() << '\n';
        output << "branch." << index << ".capacitance_F=" << branch.capacitance.si() << '\n';
        output << "branch." << index << ".inductance_H=" << branch.inductance.si() << '\n';
        output << "branch." << index << ".damping_resistance_Ohm=" << branch.dampingResistance.si() << '\n';
        output << "branch." << index << ".target_reactive_power_var=" << branch.targetReactivePower.si() << '\n';
        output << "branch." << index << ".quality_factor=" << branch.qualityFactor << '\n';
    }

    return Result<std::string>::success(output.str());
}

} // namespace p135::application
