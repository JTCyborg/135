#include "p135/domain/mechanical_geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

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
    const auto dimensionsMatch = [](double actual, double expected) {
        return std::abs(actual - expected) <= std::max(1.0e-9, std::abs(expected) * 1.0e-6);
    };
    if (!dimensionsMatch(g.yokeThicknesses[0].si(), g.yokeThicknesses[1].si()) ||
        !dimensionsMatch(g.yokeThicknesses[2].si(), g.yokeThicknesses[3].si()))
        return invalid("The two yoke halves on each horizontal level must have matching thickness for a planar middle-limb butt interface.",
            "core.yokeThicknesses");
    const double expectedAc = g.limbCenterSpacings[0].si() + g.limbCenterSpacings[1].si();
    if (std::abs(g.limbCenterSpacings[2].si() - expectedAc) >
        std::max(1.0e-9, expectedAc * 1.0e-6))
        return invalid("A-C center spacing must equal A-B plus B-C within dimensional tolerance.", "core.limbCenterSpacings[2]");
    if (!positive(g.coreOverallHeight) || !positive(g.coreOverallWidth))
        return invalid("Overall core width and height are required.", "core.overallDimensions");

    // The physical windows define the horizontal stack-up; the overall width
    // and center spacings must agree with those explicit dimensions.
    const double expectedWidth = g.limbWidths[0].si() + g.windowWidths[0].si() +
        g.limbWidths[1].si() + g.windowWidths[1].si() + g.limbWidths[2].si();
    if (!dimensionsMatch(g.coreOverallWidth.si(), expectedWidth))
        return invalid("Overall core width must equal the three limb widths plus both clear window widths.", "core.coreOverallWidth");

    const double expectedSpacingAB = 0.5 * (g.limbWidths[0].si() + g.limbWidths[1].si()) + g.windowWidths[0].si();
    const double expectedSpacingBC = 0.5 * (g.limbWidths[1].si() + g.limbWidths[2].si()) + g.windowWidths[1].si();
    if (!dimensionsMatch(g.limbCenterSpacings[0].si(), expectedSpacingAB))
        return invalid("A-B center spacing must equal half the adjacent limb widths plus the left clear window.", "core.limbCenterSpacings[0]");
    if (!dimensionsMatch(g.limbCenterSpacings[1].si(), expectedSpacingBC))
        return invalid("B-C center spacing must equal half the adjacent limb widths plus the right clear window.", "core.limbCenterSpacings[1]");

    const double expectedLeftWindowHeight = g.coreOverallHeight.si() -
        g.yokeThicknesses[0].si() - g.yokeThicknesses[2].si();
    const double expectedRightWindowHeight = g.coreOverallHeight.si() -
        g.yokeThicknesses[1].si() - g.yokeThicknesses[3].si();
    if (expectedLeftWindowHeight <= 0.0 || !dimensionsMatch(g.windowHeights[0].si(), expectedLeftWindowHeight))
        return invalid("Left clear window height must match overall height minus its top and bottom yoke thicknesses.", "core.windowHeights[0]");
    if (expectedRightWindowHeight <= 0.0 || !dimensionsMatch(g.windowHeights[1].si(), expectedRightWindowHeight))
        return invalid("Right clear window height must match overall height minus its top and bottom yoke thicknesses.", "core.windowHeights[1]");

    // Each yoke segment meets two limbs, yielding eight independently
    // specified interfaces. A Step-Lap packet stack is measured in a common
    // front-to-back depth datum and must cover the complete shared stack depth.
    struct JointDepths { double limb; double yoke; };
    const std::array<JointDepths, 8> jointDepths{{
        {g.limbDepths[0].si(), g.yokeDepths[0].si()},
        {g.limbDepths[1].si(), g.yokeDepths[0].si()},
        {g.limbDepths[1].si(), g.yokeDepths[1].si()},
        {g.limbDepths[2].si(), g.yokeDepths[1].si()},
        {g.limbDepths[0].si(), g.yokeDepths[2].si()},
        {g.limbDepths[1].si(), g.yokeDepths[2].si()},
        {g.limbDepths[1].si(), g.yokeDepths[3].si()},
        {g.limbDepths[2].si(), g.yokeDepths[3].si()}
    }};
    if (g.joints.size() != jointDepths.size())
        return invalid("All eight limb-to-yoke interfaces must have explicit joint construction records.", "core.joints");
    std::array<bool, 8> seenJoints{};
    for (std::size_t i = 0; i < g.joints.size(); ++i) {
        const auto& joint = g.joints[i];
        const auto interfaceIndex = static_cast<std::size_t>(joint.interface);
        const std::string field = "core.joints[" + std::to_string(i) + "]";
        if (interfaceIndex >= jointDepths.size())
            return invalid("Joint record references an unknown limb-to-yoke interface.", field + ".interface");
        if (seenJoints[interfaceIndex])
            return invalid("Each physical limb-to-yoke interface must be specified exactly once.", field + ".interface");
        seenJoints[interfaceIndex] = true;
        if (joint.construction == CoreJointType::Butt) {
            if (!joint.stepLapPackets.empty())
                return invalid("Butt joints must not contain Step-Lap packet geometry.", field + ".stepLapPackets");
            const double depthTolerance = std::max(1.0e-9,
                std::max(jointDepths[interfaceIndex].limb, jointDepths[interfaceIndex].yoke) * 1.0e-6);
            if (std::abs(jointDepths[interfaceIndex].limb -
                         jointDepths[interfaceIndex].yoke) > depthTolerance)
                return invalid("Butt joint members must have matching physical stack depth for full-face contact.",
                    field + ".stackDepth");
            continue;
        }
        if (joint.construction != CoreJointType::StepLap)
            return invalid("Unsupported core joint construction.", field + ".construction");
        if (joint.stepLapPackets.empty())
            return invalid("Step-Lap joints require explicit packet geometry.", field + ".stepLapPackets");
        const double availableDepth = std::min(
            jointDepths[interfaceIndex].limb, jointDepths[interfaceIndex].yoke);
        std::vector<std::pair<double, double>> packetIntervals;
        packetIntervals.reserve(joint.stepLapPackets.size());
        for (std::size_t packetIndex = 0; packetIndex < joint.stepLapPackets.size(); ++packetIndex) {
            const auto& packet = joint.stepLapPackets[packetIndex];
            const std::string packetField = field + ".stepLapPackets[" + std::to_string(packetIndex) + "]";
            if (!nonNegative(packet.stackDepthOffset) || !positive(packet.stackDepth) ||
                !positive(packet.overlapIntoLimb) || !positive(packet.overlapIntoYoke))
                return invalid("Each Step-Lap packet requires a non-negative stack offset, positive stack depth and positive overlaps into both members.", packetField);
            const double start = packet.stackDepthOffset.si();
            const double end = start + packet.stackDepth.si();
            if (!std::isfinite(end) || end > availableDepth + std::max(1.0e-9, availableDepth * 1.0e-6))
                return invalid("Step-Lap packet exceeds the shared physical stack depth.", packetField);
            packetIntervals.push_back({start, end});
        }
        std::sort(packetIntervals.begin(), packetIntervals.end());
        const double tolerance = std::max(1.0e-9, availableDepth * 1.0e-6);
        if (std::abs(packetIntervals.front().first) > tolerance ||
            std::abs(packetIntervals.back().second - availableDepth) > tolerance)
            return invalid("Step-Lap packets must cover the complete shared stack depth.", field + ".stepLapPackets");
        for (std::size_t packetIndex = 1; packetIndex < packetIntervals.size(); ++packetIndex) {
            if (std::abs(packetIntervals[packetIndex].first - packetIntervals[packetIndex - 1].second) > tolerance)
                return invalid("Step-Lap packet depth intervals must be contiguous and non-overlapping.", field + ".stepLapPackets");
        }
    }

    const double limbLength = g.coreOverallHeight.si() -
        0.5 * (g.yokeThicknesses[0].si() + g.yokeThicknesses[1].si()) -
        0.5 * (g.yokeThicknesses[2].si() + g.yokeThicknesses[3].si());
    // Yoke CAD solids extend from the outside face of the first limb to
    // the outside face of the second limb. Air-gap coordinates use that same
    // physical segment datum, including the portions spanning both limbs.
    // The two yoke halves meet at the centre plane of the middle limb.
    // AB starts at the outside face of A and ends at the centre of B;
    // BC starts at the centre of B and ends at the outside face of C.
    const double yokeABLength = g.limbWidths[0].si() + g.windowWidths[0].si() +
        0.5 * g.limbWidths[1].si();
    const double yokeBCLength = 0.5 * g.limbWidths[1].si() + g.windowWidths[1].si() +
        g.limbWidths[2].si();
    const std::array<double, 7> segmentLengths{{
        limbLength, limbLength, limbLength,
        yokeABLength, yokeBCLength, yokeABLength, yokeBCLength
    }};
    std::array<std::vector<std::pair<double, double>>, 7> intervals;
    for (std::size_t i = 0; i < g.airGaps.size(); ++i) {
        const auto& gap = g.airGaps[i];
        const auto segmentIndex = static_cast<std::size_t>(gap.segment);
        const std::string field = "core.airGaps[" + std::to_string(i) + "]";
        if (segmentIndex >= segmentLengths.size())
            return invalid("Air gap must reference one of the seven physical core segments.", field + ".segment");
        if (!positive(gap.length) || !positive(gap.positionFromSegmentStart))
            return invalid("Air gap requires a positive length and a positive position from the segment start.", field);
        const double start = gap.positionFromSegmentStart.si();
        const double end = start + gap.length.si();
        if (!std::isfinite(end) || end >= segmentLengths[segmentIndex])
            return invalid("Air gap must lie strictly inside its referenced core segment.", field);
        intervals[segmentIndex].push_back({start, end});
    }
    for (auto& segmentGaps : intervals) {
        std::sort(segmentGaps.begin(), segmentGaps.end());
        for (std::size_t i = 1; i < segmentGaps.size(); ++i) {
            if (segmentGaps[i].first < segmentGaps[i - 1].second)
                return invalid("Air gaps on the same core segment must not overlap.", "core.airGaps");
        }
    }
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
