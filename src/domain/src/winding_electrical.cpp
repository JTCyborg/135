#include "p135/domain/winding_electrical.hpp"

#include <cmath>
#include <set>
#include <string>
#include <utility>

namespace p135::domain {
namespace {

Error windingError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "WINDING_COPPER_LOSS_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

} // namespace

Result<WindingCopperLossResult> calculateWindingCopperLoss(
    const WindingCopperLossRequest& request)
{
    const auto materialValid = validate(request.material);
    if (!materialValid)
        return Result<WindingCopperLossResult>::failure(materialValid.error());

    if (request.turns == 0 || request.parallelConductors == 0)
        return Result<WindingCopperLossResult>::failure(windingError(
            ErrorCode::InvalidInput, "Winding turns and parallel conductor count must be positive.",
            "turns"));
    if (!finitePositive(request.meanTurnLength.si()) ||
        !finitePositive(request.bareConductorCrossSection.si()) ||
        !finitePositive(request.windingTemperature.si()))
        return Result<WindingCopperLossResult>::failure(windingError(
            ErrorCode::InvalidInput,
            "Mean turn length, bare conductor area and absolute winding temperature must be positive.",
            "conductorGeometry"));
    if (request.harmonicCurrents.empty())
        return Result<WindingCopperLossResult>::failure(windingError(
            ErrorCode::MissingRequiredData, "At least one harmonic RMS current is required.",
            "harmonicCurrents"));

    std::set<std::uint32_t> seenOrders;
    for (const auto& harmonic : request.harmonicCurrents) {
        if (harmonic.order == 0 || !seenOrders.insert(harmonic.order).second ||
            !finitePositive(harmonic.frequency.si()))
            return Result<WindingCopperLossResult>::failure(windingError(
                ErrorCode::InvalidInput,
                "Harmonic orders must be unique and positive and frequencies must be positive.",
                "harmonicCurrents"));
        for (const auto& current : harmonic.phaseRmsCurrent) {
            if (!std::isfinite(current.real()) || !std::isfinite(current.imag()))
                return Result<WindingCopperLossResult>::failure(windingError(
                    ErrorCode::InvalidInput, "Harmonic RMS current phasors must be finite.",
                    "harmonicCurrents"));
        }
    }

    const double rho20 = request.material.resistivityAt20C.si();
    const double alpha = request.material.temperatureCoefficient.si();
    const double deltaTemperature = request.windingTemperature.si() - 293.15;
    const double temperatureFactor = 1.0 + alpha * deltaTemperature;
    if (!finitePositive(temperatureFactor))
        return Result<WindingCopperLossResult>::failure(windingError(
            ErrorCode::InvalidInput,
            "Temperature correction produced a non-positive conductor resistivity.",
            "windingTemperature"));

    const double rhoOperating = rho20 * temperatureFactor;
    const double totalLength = static_cast<double>(request.turns) *
                               request.meanTurnLength.si();
    const double totalParallelArea =
        static_cast<double>(request.parallelConductors) *
        request.bareConductorCrossSection.si();
    const double resistance20 = rho20 * totalLength / totalParallelArea;
    const double resistanceOperating = rhoOperating * totalLength / totalParallelArea;
    if (!finitePositive(resistance20) || !finitePositive(resistanceOperating))
        return Result<WindingCopperLossResult>::failure(windingError(
            ErrorCode::InvalidInput, "Calculated winding resistance is not finite and positive.",
            "resistance"));

    WindingCopperLossResult result;
    result.resistanceAt20C = units::Resistance{resistance20};
    result.resistanceAtOperatingTemperature = units::Resistance{resistanceOperating};
    result.resistivityAtOperatingTemperature = units::Resistivity{rhoOperating};
    result.totalCopperLengthPerParallelPath = units::Length{totalLength};

    double totalLoss = 0.0;
    for (const auto& harmonic : request.harmonicCurrents) {
        WindingHarmonicCopperLoss harmonicLoss;
        harmonicLoss.order = harmonic.order;
        harmonicLoss.frequency = harmonic.frequency;
        double harmonicTotal = 0.0;
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const double currentSquared = std::norm(harmonic.phaseRmsCurrent[phase]);
            const double phaseLoss = currentSquared * resistanceOperating;
            harmonicLoss.phaseLoss[phase] = units::Power{phaseLoss};
            harmonicTotal += phaseLoss;
        }
        harmonicLoss.totalThreePhaseLoss = units::Power{harmonicTotal};
        totalLoss += harmonicTotal;
        result.harmonicLosses.push_back(std::move(harmonicLoss));
    }

    if (!std::isfinite(totalLoss))
        return Result<WindingCopperLossResult>::failure(windingError(
            ErrorCode::InvalidInput, "Calculated copper loss is not finite.", "loss"));
    result.totalThreePhaseCopperLoss = units::Power{totalLoss};
    return Result<WindingCopperLossResult>::success(std::move(result));
}

} // namespace p135::domain
