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
        const double limbDepth = jointDepths[interfaceIndex].limb;
        const double yokeDepth = jointDepths[interfaceIndex].yoke;
        const double depthTolerance = std::max(1.0e-9, std::max(limbDepth, yokeDepth) * 1.0e-6);
        if (std::abs(limbDepth - yokeDepth) > depthTolerance)
            return invalid("Step-Lap members must share the same physical stack depth; independent depth offsets are not defined by this contract.",
                field + ".stackDepth");
        const double availableDepth = limbDepth;
        const std::size_t yokeIndex = interfaceIndex < 2 ? 0 :
            (interfaceIndex < 4 ? 1 : (interfaceIndex < 6 ? 2 : 3));
        const std::size_t windowIndex = (interfaceIndex % 4) < 2 ? 0 : 1;
        // The overlap dimensions are measured normal to the butt plane:
        // into the clear limb window and into the corresponding yoke thickness.
        const double limbExtent = g.windowHeights[windowIndex].si();
        const double yokeExtent = g.yokeThicknesses[yokeIndex].si();
        const double tolerance = std::max(1.0e-9, availableDepth * 1.0e-6);
        double previousEnd = 0.0;
        StepLapPacketDirection previousDirection = StepLapPacketDirection::LimbIntoYoke;
        for (std::size_t packetIndex = 0; packetIndex < joint.stepLapPackets.size(); ++packetIndex) {
            const auto& packet = joint.stepLapPackets[packetIndex];
            const std::string packetField = field + ".stepLapPackets[" + std::to_string(packetIndex) + "]";
            if (!nonNegative(packet.stackDepthOffset) || !positive(packet.stackDepth) ||
                !positive(packet.overlapIntoLimb) || !positive(packet.overlapIntoYoke))
                return invalid("Each Step-Lap packet requires a non-negative stack offset, positive stack depth and positive overlaps into both members.", packetField);
            const double start = packet.stackDepthOffset.si();
            const double end = start + packet.stackDepth.si();
            if (!std::isfinite(end) || end > availableDepth + tolerance)
                return invalid("Step-Lap packet exceeds the shared physical stack depth.", packetField);
            if (packet.overlapIntoLimb.si() > limbExtent + tolerance ||
                packet.overlapIntoYoke.si() > yokeExtent + tolerance)
                return invalid("Step-Lap overlaps must fit inside the clear limb-window height and the corresponding yoke thickness.", packetField);
            if (packet.direction != StepLapPacketDirection::LimbIntoYoke &&
                packet.direction != StepLapPacketDirection::YokeIntoLimb)
                return invalid("Step-Lap packet direction is not supported.", packetField + ".direction");
            if (packetIndex == 0) {
                if (std::abs(start) > tolerance)
                    return invalid("The first Step-Lap packet must start at the common stack-depth datum.", packetField + ".stackDepthOffset");
            } else {
                if (std::abs(start - previousEnd) > tolerance)
                    return invalid("Step-Lap packet intervals must be supplied in depth order and be contiguous without overlap.", packetField + ".stackDepthOffset");
                if (packet.direction == previousDirection)
                    return invalid("Adjacent Step-Lap packets must alternate their limb/yoke extension direction.", packetField + ".direction");
            }
            previousEnd = end;
            previousDirection = packet.direction;
        }
        if (std::abs(previousEnd - availableDepth) > tolerance)
            return invalid("Step-Lap packets must cover the complete shared stack depth.", field + ".stepLapPackets");
    }

    const double limbLength = g.coreOverallHeight.si() -
        0.5 * (g.yokeThicknesses[0].si() + g.yokeThicknesses[1].si()) -
        0.5 * (g.yokeThicknesses[2].si() + g.yokeThicknesses[3].si());
    // Each horizontal yoke is represented by two non-overlapping halves
    // meeting at the centre plane of the middle limb. AB starts at the
    // outside face of A and ends at the centre of B; BC starts at the
    // centre of B and ends at the outside face of C. Air-gap coordinates
    // use these exact physical CAD segment datums.
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
    if (g.flangeThickness.si() > 0.0) {
        if (!positive(g.flangeOuterWidth) || !positive(g.flangeOuterHeight))
            return invalid("A bobbin with physical flanges requires explicit positive outer width and height.", "bobbin.flangeOuterDimensions");
        const double bodyWidth = g.innerWidth.si() + 2.0 * g.wallThickness.si();
        const double bodyHeight = g.innerHeight.si() + 2.0 * g.wallThickness.si();
        if (g.flangeOuterWidth.si() < bodyWidth || g.flangeOuterHeight.si() < bodyHeight)
            return invalid("Bobbin flange outer dimensions must enclose the tube outer dimensions.", "bobbin.flangeOuterDimensions");
        if (2.0 * g.flangeThickness.si() > g.axialLength.si())
            return invalid("The two bobbin flanges must fit within the axial length.", "bobbin.flangeThickness");
    }
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

    const bool hasConductorDetail = g.turns.has_value() || g.turnsPerLayer.has_value() ||
        g.parallelConductors.has_value() || !g.conductorShape.empty() ||
        g.bareConductorDiameter.has_value() || g.insulatedConductorDiameter.has_value();
    if (hasConductorDetail) {
        if (!g.turns || !g.turnsPerLayer || !g.parallelConductors ||
            g.conductorShape.empty() || !g.bareConductorDiameter ||
            !g.insulatedConductorDiameter)
            return invalid("Detailed winding geometry requires turns, turns per layer, parallel conductors, conductor shape and both conductor diameters.",
                "windingPack.conductorGeometry");
        if (*g.turns == 0 || *g.turnsPerLayer == 0 || *g.parallelConductors == 0 ||
            *g.turns != g.layerCount * *g.turnsPerLayer)
            return invalid("Turns must equal layer count multiplied by turns per layer; all conductor counts must be positive.",
                "windingPack.turns");
        if (g.conductorShape != "round")
            return invalid("Detailed CAD winding generation currently supports only explicitly dimensioned round conductors.",
                "windingPack.conductorShape");
        if (!positive(*g.bareConductorDiameter) ||
            !positive(*g.insulatedConductorDiameter) ||
            g.insulatedConductorDiameter->si() < g.bareConductorDiameter->si())
            return invalid("Round conductor diameters must be positive and insulated diameter cannot be smaller than bare diameter.",
                "windingPack.conductorDiameter");
        const double expectedInsulation = 0.5 *
            (g.insulatedConductorDiameter->si() - g.bareConductorDiameter->si());
        const double tolerance = std::max(1.0e-9, expectedInsulation * 1.0e-6);
        if (std::abs(expectedInsulation - g.conductorInsulationBuild.si()) > tolerance)
            return invalid("Conductor insulation build must equal half the difference between insulated and bare diameters.",
                "windingPack.conductorInsulationBuild");
    }
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

        const auto& bobbinGeometry = g.bobbins[i];
        const auto& windingGeometry = g.windingPacks[i];
        if (bobbinGeometry.innerWidth.si() < g.core.limbWidths[i].si())
            return invalid("Bobbin X opening must not be narrower than its core limb.", "bobbins[" + std::to_string(i) + "].innerWidth");
        if (bobbinGeometry.innerHeight.si() < g.core.limbDepths[i].si())
            return invalid("Bobbin Z opening must not be shallower than its core limb stack depth.", "bobbins[" + std::to_string(i) + "].innerHeight");

        const double availableAxialWindow = (i == 0)
            ? g.core.windowHeights[0].si()
            : ((i == 2) ? g.core.windowHeights[1].si()
                        : std::min(g.core.windowHeights[0].si(), g.core.windowHeights[1].si()));
        if (bobbinGeometry.axialLength.si() > availableAxialWindow +
            std::max(1.0e-9, availableAxialWindow * 1.0e-6))
            return invalid("Bobbin axial length along Y must fit inside the corresponding core window height.",
                "bobbins[" + std::to_string(i) + "].axialLength");

        const double requiredWindingLength = windingGeometry.axialHeight.si() +
            2.0 * windingGeometry.endClearance.si();
        if (requiredWindingLength > bobbinGeometry.axialLength.si() +
            std::max(1.0e-9, bobbinGeometry.axialLength.si() * 1.0e-6))
            return invalid("Winding axial height plus both end clearances must fit inside the bobbin axial length.",
                "windingPacks[" + std::to_string(i) + "].axialHeight");
    }
    return Result<void>::success();
}

} // namespace p135::domain
