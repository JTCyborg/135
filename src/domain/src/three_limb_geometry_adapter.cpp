#include "p135/domain/three_limb_geometry_adapter.hpp"

#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace p135::domain {
namespace {

Result<ThreeLimbMagneticGeometry> invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "THREE_LIMB_GEOMETRY_ADAPTER_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<ThreeLimbMagneticGeometry>::failure(std::move(error));
}

} // namespace

Result<units::Area> calculateLaminatedLimbEffectiveArea(
    units::Length limbWidth, units::Length stackDepth, double stackingFactor)
{
    if (!std::isfinite(limbWidth.si()) || limbWidth.si() <= 0.0)
        return Result<units::Area>::failure(invalid(
            "Limb width must be finite and greater than zero.", "limbWidth").error());
    if (!std::isfinite(stackDepth.si()) || stackDepth.si() <= 0.0)
        return Result<units::Area>::failure(invalid(
            "Core stack depth must be finite and greater than zero.", "stackDepth").error());
    if (!std::isfinite(stackingFactor) || stackingFactor <= 0.0 || stackingFactor > 1.0)
        return Result<units::Area>::failure(invalid(
            "Stacking factor must be finite and in the interval (0, 1].", "stackingFactor").error());

    const double effectiveArea = limbWidth.si() * stackDepth.si() * stackingFactor;
    if (!std::isfinite(effectiveArea) || effectiveArea <= 0.0)
        return Result<units::Area>::failure(invalid(
            "Calculated laminated limb area is not finite and positive.", "effectiveArea").error());
    return Result<units::Area>::success(units::Area{effectiveArea});
}

Result<ThreeLimbMagneticGeometry> buildThreeLimbMagneticGeometry(
    const ThreeLimbCorePhysicalGeometry& physical,
    double stackingFactor)
{
    const auto validation = validate(physical);
    if (!validation)
        return Result<ThreeLimbMagneticGeometry>::failure(validation.error());

    if (!std::isfinite(stackingFactor) ||
        stackingFactor <= 0.0 || stackingFactor > 1.0)
        return invalid("Stacking factor must be finite and in the interval (0, 1].",
                       "stackingFactor");

    ThreeLimbMagneticGeometry result;
    for (std::size_t limb = 0; limb < 3; ++limb) {
        const double grossArea =
            physical.limbWidths[limb].si() * physical.limbDepths[limb].si();
        const double effectiveArea = grossArea * stackingFactor;
        if (!std::isfinite(effectiveArea) || effectiveArea <= 0.0)
            return invalid("Calculated limb effective area is not finite and positive.",
                           "limbEffectiveAreas[" + std::to_string(limb) + "]");

        result.limbEffectiveAreas[limb] = units::Area{effectiveArea};
        result.limbGapEffectiveAreas[limb] = units::Area{effectiveArea};

        // The centre limb joins both AB and BC yokes. Its centre-line datum
        // therefore uses the mean thickness of the two top and two bottom
        // yoke segments instead of arbitrarily selecting the AB segment.
        const double topYokeThickness = limb == 0
            ? physical.yokeThicknesses[0].si()
            : limb == 1
                ? 0.5 * (physical.yokeThicknesses[0].si() +
                         physical.yokeThicknesses[1].si())
                : physical.yokeThicknesses[1].si();
        const double bottomYokeThickness = limb == 0
            ? physical.yokeThicknesses[2].si()
            : limb == 1
                ? 0.5 * (physical.yokeThicknesses[2].si() +
                         physical.yokeThicknesses[3].si())
                : physical.yokeThicknesses[3].si();
        const double path = physical.coreOverallHeight.si() -
            0.5 * topYokeThickness - 0.5 * bottomYokeThickness;
        if (!std::isfinite(path) || path <= 0.0)
            return invalid("Derived limb centre-line path must be finite and positive.",
                           "limbMeanPaths[" + std::to_string(limb) + "]");
        // Air gaps are represented by a separate reluctance in the magnetic
        // solver. Keep only the remaining ferromagnetic length in this branch
        // to avoid counting the same physical gap once as steel and again as air.
        result.limbMeanPaths[limb] = units::Length{path};
    }

    // The solver has four independent yoke branches: top AB, top BC,
    // bottom AB and bottom BC. Their centre-line paths follow the explicit
    // limb centre spacings in the physical drawing.
    for (std::size_t yoke = 0; yoke < 4; ++yoke) {
        const double grossArea =
            physical.yokeThicknesses[yoke].si() * physical.yokeDepths[yoke].si();
        const double effectiveArea = grossArea * stackingFactor;
        if (!std::isfinite(effectiveArea) || effectiveArea <= 0.0)
            return invalid("Calculated yoke effective area is not finite and positive.",
                           "yokeEffectiveAreas[" + std::to_string(yoke) + "]");
        result.yokeEffectiveAreas[yoke] = units::Area{effectiveArea};
        result.yokeSegmentPaths[yoke] = physical.limbCenterSpacings[yoke % 2];
    }

    std::array<double, 3> totalLimbGap{};
    for (std::size_t index = 0; index < physical.airGaps.size(); ++index) {
        const auto& gap = physical.airGaps[index];
        std::size_t limb = 0;
        switch (gap.segment) {
        case CoreSegment::LimbA: limb = 0; break;
        case CoreSegment::LimbB: limb = 1; break;
        case CoreSegment::LimbC: limb = 2; break;
        case CoreSegment::YokeTopAB:
        case CoreSegment::YokeTopBC:
        case CoreSegment::YokeBottomAB:
        case CoreSegment::YokeBottomBC:
            return invalid(
                "The current nonlinear magnetic solver models air gaps in limb branches only; "
                "a yoke-segment air gap cannot be mapped without extending the solver.",
                "airGaps[" + std::to_string(index) + "].segment");
        }

        totalLimbGap[limb] += gap.length.si();
        if (!std::isfinite(totalLimbGap[limb]))
            return invalid("Total air-gap length is not finite.",
                           "airGaps[" + std::to_string(index) + "].length");
    }

    for (std::size_t limb = 0; limb < 3; ++limb) {
        const double physicalPath = result.limbMeanPaths[limb]->si();
        const double ironPath = physicalPath - totalLimbGap[limb];
        if (!std::isfinite(ironPath) || ironPath <= 0.0)
            return invalid(
                "Total air-gap length must be smaller than the physical limb centre-line path.",
                "airGaps.limb[" + std::to_string(limb) + "]");
        result.limbMeanPaths[limb] = units::Length{ironPath};
        result.limbAirGaps[limb] = units::Length{totalLimbGap[limb]};
    }

    return Result<ThreeLimbMagneticGeometry>::success(std::move(result));
}

} // namespace p135::domain
