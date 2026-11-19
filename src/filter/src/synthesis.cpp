#include "p135/filter/synthesis.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace p135::filter {
namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

Result<FilterDesign> invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "FILTER_INPUT_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<FilterDesign>::failure(std::move(error));
}

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

Result<FilterDesign> synthesize(const FilterSpecification& specification)
{
    const double lineVoltage = specification.network.lineToLineVoltage.si();
    const double fundamental = specification.network.fundamentalFrequency.si();

    if (!finitePositive(lineVoltage))
        return invalid("Line-to-line voltage must be finite and greater than zero.",
                       "network.lineToLineVoltage");
    if (!finitePositive(fundamental))
        return invalid("Fundamental frequency must be finite and greater than zero.",
                       "network.fundamentalFrequency");
    if (specification.network.connection != Connection::Wye &&
        specification.network.connection != Connection::Delta)
        return invalid("Connection must be Wye or Delta.", "network.connection");
    if (specification.branches.empty())
        return invalid("At least one single-tuned branch is required.", "branches");

    const double phaseVoltage = specification.network.connection == Connection::Wye
        ? lineVoltage / std::sqrt(3.0)
        : lineVoltage;

    FilterDesign design;
    design.network = specification.network;
    design.branches.reserve(specification.branches.size());

    std::unordered_set<std::uint32_t> ids;
    const double omega1 = kTwoPi * fundamental;

    for (std::size_t index = 0; index < specification.branches.size(); ++index) {
        const auto& branch = specification.branches[index];
        const std::string prefix = "branches[" + std::to_string(index) + "]";

        if (!ids.insert(branch.id).second)
            return invalid("Branch identifiers must be unique.", prefix + ".id");
        if (branch.tuningHarmonicOrder < 2)
            return invalid("Tuning harmonic order must be at least 2.",
                           prefix + ".tuningHarmonicOrder");
        if (!finitePositive(branch.targetReactivePower.si()))
            return invalid("Target reactive power must be finite and greater than zero.",
                           prefix + ".targetReactivePower");
        if (!finitePositive(branch.qualityFactor))
            return invalid("Quality factor must be finite and greater than zero.",
                           prefix + ".qualityFactor");

        const double harmonicOrder = static_cast<double>(branch.tuningHarmonicOrder);
        const double tuningFrequency = fundamental * harmonicOrder;
        const double omegaTuning = kTwoPi * tuningFrequency;
        const double qPerPhase = branch.targetReactivePower.si() / 3.0;
        const double detuningFactor = 1.0 - 1.0 / (harmonicOrder * harmonicOrder);

        // Fundamental-frequency reactive power of a series LC branch:
        // Q = U_phase^2 * omega1 * C / (1 - (f1 / ftune)^2).
        const double capacitance =
            qPerPhase * detuningFactor / (omega1 * phaseVoltage * phaseVoltage);
        const double inductance =
            1.0 / (omegaTuning * omegaTuning * capacitance);
        const double resistance =
            omegaTuning * inductance / branch.qualityFactor;

        if (!finitePositive(capacitance) || !finitePositive(inductance) ||
            !finitePositive(resistance))
            return invalid("Calculated component value is outside the supported numeric domain.",
                           prefix);

        design.branches.push_back(SingleTunedBranchDesign{
            branch.id,
            branch.name,
            branch.tuningHarmonicOrder,
            units::Voltage{phaseVoltage},
            units::Capacitance{capacitance},
            units::Inductance{inductance},
            units::Resistance{resistance},
            branch.targetReactivePower,
            branch.qualityFactor
        });
    }

    return Result<FilterDesign>::success(std::move(design));
}

} // namespace p135::filter
