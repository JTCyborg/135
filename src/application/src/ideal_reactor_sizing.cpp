#include "p135/application/ideal_reactor_sizing.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace p135::application {
namespace {
constexpr double vacuumPermeability = 1.25663706212e-6;

Result<IdealReactorSizingResult> invalid(const char* message)
{
    return Result<IdealReactorSizingResult>::failure(
        {ErrorCode::InvalidInput, message, {}});
}
}

Result<IdealReactorSizingResult> calculateIdealReactorAreaProduct(
    const IdealReactorSizingRequest& request)
{
    const double values[] = {request.inductanceH, request.peakCurrentA,
        request.rmsCurrentA, request.allowablePeakFluxDensityT,
        request.allowableCurrentDensityApm2, request.windowUtilization,
        request.stackingFactor, request.windowToCoreAreaRatio};
    for (const double value : values) {
        if (!std::isfinite(value) || value <= 0.0)
            return invalid("Ideal sizing inputs must be finite and greater than zero.");
    }
    if (request.windowUtilization > 1.0)
        return invalid("Window utilization must be in the interval (0, 1].");
    if (request.stackingFactor > 1.0)
        return invalid("Stacking factor must be in the interval (0, 1].");

    const double effectiveAreaProduct = request.inductanceH * request.peakCurrentA *
        request.rmsCurrentA / (request.allowablePeakFluxDensityT *
        request.allowableCurrentDensityApm2 * request.windowUtilization);
    const double grossAreaProduct = effectiveAreaProduct / request.stackingFactor;
    if (!std::isfinite(effectiveAreaProduct) || !std::isfinite(grossAreaProduct) ||
        effectiveAreaProduct <= 0.0 || grossAreaProduct <= 0.0)
        return invalid("Ideal sizing area product is outside the representable numeric range.");

    const double grossCoreArea = std::sqrt(grossAreaProduct / request.windowToCoreAreaRatio);
    const double effectiveCoreArea = grossCoreArea * request.stackingFactor;
    const double windowArea = grossCoreArea * request.windowToCoreAreaRatio;
    const double turnsReal = request.inductanceH * request.peakCurrentA /
        (request.allowablePeakFluxDensityT * effectiveCoreArea);
    if (!std::isfinite(grossCoreArea) || !std::isfinite(effectiveCoreArea) ||
        !std::isfinite(windowArea) || !std::isfinite(turnsReal) ||
        grossCoreArea <= 0.0 || effectiveCoreArea <= 0.0 || windowArea <= 0.0 ||
        turnsReal < 1.0 || turnsReal >= static_cast<double>(
            std::numeric_limits<std::uint64_t>::max()))
        return invalid("Ideal core area or turn count is outside the representable range.");

    const auto turns = static_cast<std::uint64_t>(std::ceil(turnsReal));
    const double totalGap = vacuumPermeability * static_cast<double>(turns) *
        static_cast<double>(turns) * effectiveCoreArea / request.inductanceH;
    if (!std::isfinite(totalGap) || totalGap <= 0.0)
        return invalid("Ideal air-gap estimate is outside the representable numeric range.");

    return Result<IdealReactorSizingResult>::success({
        effectiveAreaProduct, grossAreaProduct, std::sqrt(grossAreaProduct),
        0.5 * request.inductanceH * request.peakCurrentA * request.peakCurrentA,
        request.allowablePeakFluxDensityT, request.allowableCurrentDensityApm2,
        request.windowUtilization, request.stackingFactor,
        request.windowToCoreAreaRatio, grossCoreArea, effectiveCoreArea,
        windowArea, turns, totalGap
    });
}
} // namespace p135::application
