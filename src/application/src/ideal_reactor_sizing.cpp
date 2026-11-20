#include "p135/application/ideal_reactor_sizing.hpp"

#include <cmath>

namespace p135::application {
namespace {
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
        request.stackingFactor};
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
        return invalid("Ideal sizing result is outside the representable numeric range.");

    return Result<IdealReactorSizingResult>::success({effectiveAreaProduct,
        grossAreaProduct, std::sqrt(grossAreaProduct),
        0.5 * request.inductanceH * request.peakCurrentA * request.peakCurrentA,
        request.allowablePeakFluxDensityT, request.allowableCurrentDensityApm2,
        request.windowUtilization, request.stackingFactor});
}
} // namespace p135::application
