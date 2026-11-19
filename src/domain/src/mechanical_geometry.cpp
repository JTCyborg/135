#include "p135/domain/mechanical_geometry.hpp"

#include <algorithm>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace p135::domain {
namespace {

Result<void> invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "MECHANICAL_GEOMETRY_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<void>::failure(std::move(error));
}

bool positive(units::Length value)
{
    return std::isfinite(value.si()) && value.si() > 0.0;
}

bool nonNegative(units::Length value)
{
    return std::isfinite(value.si()) && value.si() >= 0.0;
}

} // namespace

Result<void> validate(const ThreeLimbCorePhysicalGeometry& g)
{
    for (std::size_t i = 0; i < g.limbWidths.size(); ++i) {
        if (!positive(g.limbWidths[i])) return invalid("Every limb requires a positive physical width.", "core.limbWidths[" + std::to_string(i) + "]");
        if (!positive(g.limbDepths[i])) return invalid("Every limb requires a positive physical stack depth.", "core.limbDepths[" + std::to_string(i) + "]");
    }
    for (std::size_t i = 0; i < g.windowWidths.size(); ++i) {
        if (!positive(g.windowWidths[i])) return invalid("Both clear window widths are required.", "core.windowWidths[" + std::to_string(i) + "]");
        if (!positive(g.windowHeights[i])) return invalid("Both clear window heights are required.", "core.windowHeights[" + std::to_string(i) + "]");
    }
    for (std::size_t i = 0; i < g.yokeThicknesses.size(); ++i) {
        if (!positive(g.yokeThicknesses[i])) return invalid("Every yoke segment requires a positive physical thickness.", "core.yokeThicknesses[" + std::to_string(i) + "]");
        if (!positive(g.yokeDepths[i])) return invalid("Every yoke segment requires a positive physical depth.", "core.yokeDepths[" + std::to_string(i) + "]");
    }
    for (std::size_t i = 0; i < g.limbCenterSpacings.size(); ++i) {
        if (!positive(g.limbCenterSpacings[i])) return invalid("Limb center spacings A-B, B-C and A-C are required.", "core.limbCenterSpacings[" + std::to_string(i) + "]");
    }
    const double expectedAc = g.limbCenterSpacings[0].si() + g.limbCenterSpacings[1].si();
    if (std::abs(g.limbCenterSpacings[2].si() - expectedAc) >
        std::max(1.0e-9, expectedAc * 1.0e-6))
        return invalid("A-C center spacing must equal A-B plus B-C within dimensional tolerance.", "core.limbCenterSpacings[2]");
    if (!positive(g.coreOverallHeight) || !positive(g.coreOverallWidth))
        return invalid("Overall core width and height are required.", "core.overallDimensions");
    for (std::size_t i = 0; i < g.airGapLengths.size(); ++i) {
        if (!nonNegative(g.airGapLengths[i])) return invalid("Air-gap lengths must be finite and non-negative.", "core.airGapLengths[" + std::to_string(i) + "]");
    }
    const bool anyGap = std::any_of(g.airGapLengths.begin(), g.airGapLengths.end(),
        [](units::Length value) { return value.si() > 0.0; });
    if (anyGap && g.airGapLocation == AirGapLocation::None)
        return invalid("A non-zero air gap requires an explicit air-gap location.", "core.airGapLocation");
    if (!anyGap && g.airGapLocation != AirGapLocation::None)
        return invalid("Air-gap location is set but all physical air-gap lengths are zero.", "core.airGapLocation");
    if (g.laminationConstruction.empty())
        return invalid("Lamination/packet construction description is required.", "core.laminationConstruction");
    return Result<void>::success();
}

Result<void> validate(const BobbinPhysicalGeometry& g)
{
    if (!positive(g.innerWidth) || !positive(g.innerHeight) || !positive(g.axialLength) ||
        !positive(g.wallThickness))
        return invalid("Bobbin inner width, inner height, axial length and wall thickness are required.", "bobbin.dimensions");
    if (!nonNegative(g.flangeThickness) || !nonNegative(g.windingClearance))
        return invalid("Bobbin flange thickness and winding clearance must be non-negative.", "bobbin.clearances");
    if (g.material.empty()) return invalid("Bobbin material is required.", "bobbin.material");
    return Result<void>::success();
}

Result<void> validate(const WindingPackPhysicalGeometry& g)
{
    if (!positive(g.radialBuild) || !positive(g.axialHeight))
        return invalid("Winding radial build and axial height are required.", "windingPack.dimensions");
    if (!nonNegative(g.endClearance) || !nonNegative(g.layerInsulationBuild) ||
        !nonNegative(g.mainInsulationBuild) || !nonNegative(g.conductorInsulationBuild))
        return invalid("Winding clearances and insulation builds must be non-negative.", "windingPack.clearances");
    if (g.layerCount == 0 || g.sections == 0)
        return invalid("Winding layer count and section count must be non-zero.", "windingPack.construction");
    if (g.constructionDescription.empty())
        return invalid("Winding construction description is required.", "windingPack.constructionDescription");
    return Result<void>::success();
}

Result<void> validate(const ReactorPhysicalGeometry& g)
{
    if (g.productId.empty()) return invalid("Product ID is required.", "productId");
    if (g.dimensionalSource.empty() || g.dimensionalRevision.empty())
        return invalid("Dimensional source and revision are required for traceability.", "dimensionalProvenance");
    const auto core = validate(g.core);
    if (!core) return core;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto bobbin = validate(g.bobbins[i]);
        if (!bobbin) return bobbin;
        const auto winding = validate(g.windingPacks[i]);
        if (!winding) return winding;
    }
    return Result<void>::success();
}

} // namespace p135::domain
