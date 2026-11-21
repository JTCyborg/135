#include "p135/cad/drawing_export.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

namespace p135::cad {
namespace {

struct DxfWriter {
    std::ofstream stream;

    explicit DxfWriter(const std::string& path)
        : stream(path, std::ios::out | std::ios::trunc)
    {
        stream << std::fixed << std::setprecision(3);
    }

    void pair(int code, const std::string& value) { stream << code << "\n" << value << "\n"; }
    void pair(int code, double value) { stream << code << "\n" << value << "\n"; }

    void line(double x1, double y1, double x2, double y2, const char* layer)
    {
        pair(0, "LINE"); pair(8, layer);
        pair(10, x1); pair(20, y1); pair(30, 0.0);
        pair(11, x2); pair(21, y2); pair(31, 0.0);
    }

    void rectangle(double x, double y, double width, double height, const char* layer)
    {
        line(x, y, x + width, y, layer);
        line(x + width, y, x + width, y + height, layer);
        line(x + width, y + height, x, y + height, layer);
        line(x, y + height, x, y, layer);
    }

    void text(double x, double y, double height, const std::string& value, const char* layer)
    {
        pair(0, "TEXT"); pair(8, layer);
        pair(10, x); pair(20, y); pair(30, 0.0);
        pair(40, height); pair(1, value);
    }

    void horizontalDimension(double x1, double x2, double baseY, double dimensionY,
        const std::string& label)
    {
        line(x1, baseY, x1, dimensionY, "DIMENSIONS");
        line(x2, baseY, x2, dimensionY, "DIMENSIONS");
        line(x1, dimensionY, x2, dimensionY, "DIMENSIONS");
        constexpr double arrow = 2.0;
        line(x1, dimensionY, x1 + arrow, dimensionY + 1.0, "DIMENSIONS");
        line(x1, dimensionY, x1 + arrow, dimensionY - 1.0, "DIMENSIONS");
        line(x2, dimensionY, x2 - arrow, dimensionY + 1.0, "DIMENSIONS");
        line(x2, dimensionY, x2 - arrow, dimensionY - 1.0, "DIMENSIONS");
        text(0.5 * (x1 + x2) - 0.5 * static_cast<double>(label.size()) * 1.2,
            dimensionY + 2.0, 2.2, label, "DIMENSIONS");
    }

    void verticalDimension(double baseX, double y1, double y2, double dimensionX,
        const std::string& label)
    {
        line(baseX, y1, dimensionX, y1, "DIMENSIONS");
        line(baseX, y2, dimensionX, y2, "DIMENSIONS");
        line(dimensionX, y1, dimensionX, y2, "DIMENSIONS");
        constexpr double arrow = 2.0;
        line(dimensionX, y1, dimensionX + 1.0, y1 + arrow, "DIMENSIONS");
        line(dimensionX, y1, dimensionX - 1.0, y1 + arrow, "DIMENSIONS");
        line(dimensionX, y2, dimensionX + 1.0, y2 - arrow, "DIMENSIONS");
        line(dimensionX, y2, dimensionX - 1.0, y2 - arrow, "DIMENSIONS");
        text(dimensionX - 5.0, 0.5 * (y1 + y2), 2.2, label, "DIMENSIONS");
    }

    void begin()
    {
        pair(0, "SECTION"); pair(2, "HEADER");
        pair(9, "$ACADVER"); pair(1, "AC1009");
        pair(9, "$INSUNITS"); pair(70, 4.0);
        pair(9, "$MEASUREMENT"); pair(70, 1.0);
        pair(0, "ENDSEC");
        pair(0, "SECTION"); pair(2, "TABLES");
        pair(0, "TABLE"); pair(2, "LAYER"); pair(70, "5");
        const auto layer = [this](const char* name, const char* color) {
            pair(0, "LAYER"); pair(2, name); pair(70, "0"); pair(62, color); pair(6, "CONTINUOUS");
        };
        layer("OUTLINE", "7");
        layer("WINDING_ENVELOPE", "4");
        layer("CENTER", "3");
        layer("DIMENSIONS", "2");
        layer("ANNOTATION", "1");
        pair(0, "ENDTAB"); pair(0, "ENDSEC");
        pair(0, "SECTION"); pair(2, "ENTITIES");
    }

    void finish()
    {
        pair(0, "ENDSEC"); pair(0, "EOF");
    }
};

Error drawingError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CAD_DRAWING_EXPORT_FAILED", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

double mm(double metres) { return metres * 1000.0; }

using GapIntervals = std::vector<std::pair<double, double>>;

GapIntervals gapIntervalsFor(
    const domain::ThreeLimbCorePhysicalGeometry& core,
    domain::CoreSegment segment)
{
    GapIntervals intervals;
    for (const auto& gap : core.airGaps) {
        if (gap.segment == segment) {
            intervals.emplace_back(
                mm(gap.positionFromSegmentStart.si()),
                mm(gap.positionFromSegmentStart.si() + gap.length.si()));
        }
    }
    std::sort(intervals.begin(), intervals.end());
    return intervals;
}

// A limb is split along its Y axis. In front elevation (X-Y), each air gap
// interrupts both side edges and is bounded by the two physical cut faces.
void verticalCoreMember(
    DxfWriter& dxf, double x, double y, double width, double height,
    const GapIntervals& gaps)
{
    double cursor = 0.0;
    for (const auto& gap : gaps) {
        const double start = std::clamp(gap.first, cursor, height);
        const double end = std::clamp(gap.second, start, height);
        dxf.line(x, y + cursor, x, y + start, "OUTLINE");
        dxf.line(x + width, y + cursor, x + width, y + start, "OUTLINE");
        dxf.line(x, y + start, x + width, y + start, "OUTLINE");
        dxf.line(x, y + end, x + width, y + end, "OUTLINE");
        cursor = end;
    }
    dxf.line(x, y + cursor, x, y + height, "OUTLINE");
    dxf.line(x + width, y + cursor, x + width, y + height, "OUTLINE");
    dxf.line(x, y, x + width, y, "OUTLINE");
    dxf.line(x, y + height, x + width, y + height, "OUTLINE");
}

// A yoke is split along its X axis. This helper is used in both front and
// plan projections so a physical yoke air gap is not drawn as solid material.
void horizontalCoreMember(
    DxfWriter& dxf, double x, double y, double width, double height,
    const GapIntervals& gaps)
{
    double cursor = 0.0;
    for (const auto& gap : gaps) {
        const double start = std::clamp(gap.first, cursor, width);
        const double end = std::clamp(gap.second, start, width);
        dxf.line(x + cursor, y, x + start, y, "OUTLINE");
        dxf.line(x + cursor, y + height, x + start, y + height, "OUTLINE");
        dxf.line(x + start, y, x + start, y + height, "OUTLINE");
        dxf.line(x + end, y, x + end, y + height, "OUTLINE");
        cursor = end;
    }
    dxf.line(x + cursor, y, x + width, y, "OUTLINE");
    dxf.line(x + cursor, y + height, x + width, y + height, "OUTLINE");
    dxf.line(x, y, x, y + height, "OUTLINE");
    dxf.line(x + width, y, x + width, y + height, "OUTLINE");
}

} // namespace

Result<void> exportReactorAssemblyDxf(
    const domain::ReactorPhysicalGeometry& geometry,
    const std::string& filePath)
{
    const auto validation = domain::validate(geometry);
    if (!validation) return validation;
    if (filePath.empty())
        return Result<void>::failure(drawingError(
            ErrorCode::InvalidInput, "A non-empty DXF output path is required.", "drawing.filePath"));

    for (std::size_t i = 0; i < geometry.core.joints.size(); ++i) {
        if (geometry.core.joints[i].construction == domain::CoreJointType::StepLap)
            return Result<void>::failure(drawingError(
                ErrorCode::UnsupportedConfiguration,
                "Step-Lap drawing details are blocked until the physical packet layout is supported by the CAD drawing generator.",
                "core.joints[" + std::to_string(i) + "]"));
    }

    DxfWriter dxf(filePath);
    if (!dxf.stream.is_open())
        return Result<void>::failure(drawingError(
            ErrorCode::SolverFailure, "Could not open the DXF output file.", "drawing.filePath"));

    dxf.begin();
    dxf.text(20.0, 570.0, 5.0, "PROJECT135 - THREE-PHASE REACTOR", "ANNOTATION");
    dxf.text(20.0, 562.0, 3.0, "PRELIMINARY / NOT VERIFIED - NOT FOR MANUFACTURE", "ANNOTATION");
    dxf.text(20.0, 555.0, 2.5, "Units: mm | Projection: orthographic | Geometry source: Construction physical dimensions", "ANNOTATION");
    dxf.text(20.0, 548.0, 2.5, "Product: " + geometry.productId, "ANNOTATION");
    dxf.text(20.0, 541.0, 2.5, "Source: " + geometry.dimensionalSource, "ANNOTATION");
    dxf.text(20.0, 534.0, 2.5, "Revision: " + geometry.dimensionalRevision, "ANNOTATION");

    const auto& core = geometry.core;
    const double wA = mm(core.limbWidths[0].si());
    const double wB = mm(core.limbWidths[1].si());
    const double wC = mm(core.limbWidths[2].si());
    const double gapAB = mm(core.windowWidths[0].si());
    const double gapBC = mm(core.windowWidths[1].si());
    const double totalWidth = mm(core.coreOverallWidth.si());
    const double totalHeight = mm(core.coreOverallHeight.si());
    const double yokeBottom = 0.5 * (mm(core.yokeThicknesses[2].si()) + mm(core.yokeThicknesses[3].si()));
    const double yokeTop = 0.5 * (mm(core.yokeThicknesses[0].si()) + mm(core.yokeThicknesses[1].si()));
    const double limbHeight = totalHeight - yokeBottom - yokeTop;
    const double xA = 0.0;
    const double xB = wA + gapAB;
    const double xC = xB + wB + gapBC;

    // Front elevation: the seven core members are drawn as their explicit
    // rectangular physical envelopes; bobbins/winding are labelled envelopes.
    const double frontX = 25.0, frontY = 285.0;
    dxf.text(frontX, frontY + totalHeight + 12.0, 3.5, "A - FRONT ELEVATION", "ANNOTATION");
    const std::array<domain::CoreSegment, 3> limbSegments{
        domain::CoreSegment::LimbA, domain::CoreSegment::LimbB, domain::CoreSegment::LimbC};
    for (std::size_t i = 0; i < limbSegments.size(); ++i) {
        verticalCoreMember(dxf, frontX + std::array<double, 3>{xA, xB, xC}[i],
            frontY + yokeBottom, mm(core.limbWidths[i].si()), limbHeight,
            gapIntervalsFor(core, limbSegments[i]));
    }

    const double xMid = xB + 0.5 * wB;
    const std::array<double, 4> yokeX{frontX + xA, frontX + xMid,
                                      frontX + xA, frontX + xMid};
    const std::array<double, 4> yokeWidths{xMid - xA, xC + wC - xMid,
                                           xMid - xA, xC + wC - xMid};
    const std::array<double, 4> yokeY{
        frontY + totalHeight - mm(core.yokeThicknesses[0].si()),
        frontY + totalHeight - mm(core.yokeThicknesses[1].si()),
        frontY, frontY};
    const std::array<domain::CoreSegment, 4> yokeSegments{
        domain::CoreSegment::YokeTopAB, domain::CoreSegment::YokeTopBC,
        domain::CoreSegment::YokeBottomAB, domain::CoreSegment::YokeBottomBC};
    for (std::size_t i = 0; i < yokeSegments.size(); ++i) {
        horizontalCoreMember(dxf, yokeX[i], yokeY[i], yokeWidths[i],
            mm(core.yokeThicknesses[i].si()), gapIntervalsFor(core, yokeSegments[i]));
    }

    const std::array<double, 3> limbX{xA, xB, xC};
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const auto& bobbin = geometry.bobbins[phase];
        const auto& winding = geometry.windingPacks[phase];
        const double bobbinLength = mm(bobbin.axialLength.si());
        const double windowHeight = mm(phase == 0 ? core.windowHeights[0].si()
            : phase == 1 ? std::min(core.windowHeights[0].si(), core.windowHeights[1].si())
                         : core.windowHeights[1].si());
        const double bobbinY = yokeBottom + 0.5 * (windowHeight - bobbinLength);
        const double flangeW = mm(bobbin.flangeOuterWidth.si());
        const double flangeH = mm(bobbin.flangeOuterHeight.si());
        const double limbCenter = limbX[phase] + 0.5 * mm(core.limbWidths[phase].si());
        const double bobbinOuterWidth = mm(bobbin.innerWidth.si() + 2.0 * bobbin.wallThickness.si());
        const double bobbinOuterDepth = mm(bobbin.innerHeight.si() + 2.0 * bobbin.wallThickness.si());
        const double windingOuterWidth = bobbinOuterWidth + 2.0 * mm(bobbin.windingClearance.si() + winding.radialBuild.si());
        const double windingAxial = mm(winding.axialHeight.si());
        const double windingY = bobbinY + 0.5 * (bobbinLength - windingAxial);
        dxf.rectangle(frontX + limbCenter - 0.5 * flangeW, frontY + bobbinY,
            flangeW, bobbinLength, "OUTLINE");
        dxf.rectangle(frontX + limbCenter - 0.5 * windingOuterWidth, frontY + windingY,
            windingOuterWidth, windingAxial, "WINDING_ENVELOPE");
        dxf.text(frontX + limbCenter - 3.0, frontY + bobbinY + bobbinLength + 3.0,
            2.0, "PHASE " + std::string(1, static_cast<char>('A' + phase)) + " - ENVELOPE", "ANNOTATION");
        (void)flangeH;
        (void)bobbinOuterDepth;
    }
    dxf.horizontalDimension(frontX, frontX + totalWidth, frontY, frontY - 20.0,
        "OVERALL WIDTH " + std::to_string(totalWidth) + " mm");
    dxf.verticalDimension(frontX, frontY, frontY + totalHeight, frontX - 18.0,
        "OVERALL HEIGHT " + std::to_string(totalHeight) + " mm");
    dxf.text(frontX, frontY - 30.0, 2.5,
        "Core overall: " + std::to_string(totalWidth) + " x " + std::to_string(totalHeight) + " mm", "DIMENSIONS");

    // Front-view dimension chains use only the explicit Construction values.
    // They intentionally describe the nominal envelopes, not toleranced parts.
    const double widthChainY = frontY - 48.0;
    dxf.horizontalDimension(frontX + xA, frontX + xA + wA, frontY,
        widthChainY, "LIMB A WIDTH " + std::to_string(wA) + " mm");
    dxf.horizontalDimension(frontX + xB, frontX + xB + wB, frontY,
        widthChainY - 15.0, "LIMB B WIDTH " + std::to_string(wB) + " mm");
    dxf.horizontalDimension(frontX + xC, frontX + xC + wC, frontY,
        widthChainY - 30.0, "LIMB C WIDTH " + std::to_string(wC) + " mm");
    dxf.horizontalDimension(frontX + xA + wA, frontX + xB, frontY,
        widthChainY - 45.0, "WINDOW AB " + std::to_string(gapAB) + " mm");
    dxf.horizontalDimension(frontX + xB + wB, frontX + xC, frontY,
        widthChainY - 60.0, "WINDOW BC " + std::to_string(gapBC) + " mm");
    dxf.text(frontX, frontY - 125.0, 2.2,
        "Yoke thickness T1/T2/B1/B2: " +
        std::to_string(mm(core.yokeThicknesses[0].si())) + " / " +
        std::to_string(mm(core.yokeThicknesses[1].si())) + " / " +
        std::to_string(mm(core.yokeThicknesses[2].si())) + " / " +
        std::to_string(mm(core.yokeThicknesses[3].si())) + " mm", "DIMENSIONS");

    // Plan view: core stack depths and the explicit bobbin/winding cross-sections.
    const double planX = 25.0, planY = 80.0;
    dxf.text(planX, planY + 115.0, 3.5, "B - PLAN VIEW (X-Z)", "ANNOTATION");
    const double maxDepth = std::max({mm(core.limbDepths[0].si()), mm(core.limbDepths[1].si()),
                                      mm(core.limbDepths[2].si()), mm(core.yokeDepths[0].si()),
                                      mm(core.yokeDepths[1].si()), mm(core.yokeDepths[2].si()),
                                      mm(core.yokeDepths[3].si())});
    // The four yoke footprints are separate physical members and each has
    // its own stack depth. Their X extents match the OCCT core builder datum.
    const std::array<double, 4> planYokeX{planX + xA, planX + xMid,
                                          planX + xA, planX + xMid};
    const std::array<double, 4> planYokeWidths{xMid - xA, xC + wC - xMid,
                                               xMid - xA, xC + wC - xMid};
    const std::array<const char*, 4> planYokeLabels{
        "YOKE TOP AB PLAN", "YOKE TOP BC PLAN",
        "YOKE BOTTOM AB PLAN", "YOKE BOTTOM BC PLAN"};
    const std::array<domain::CoreSegment, 4> planYokeSegments{
        domain::CoreSegment::YokeTopAB, domain::CoreSegment::YokeTopBC,
        domain::CoreSegment::YokeBottomAB, domain::CoreSegment::YokeBottomBC};
    for (std::size_t i = 0; i < planYokeSegments.size(); ++i) {
        const double depth = mm(core.yokeDepths[i].si());
        horizontalCoreMember(dxf, planYokeX[i], planY, planYokeWidths[i],
            depth, gapIntervalsFor(core, planYokeSegments[i]));
        dxf.text(planX, planY - 34.0 - 5.0 * static_cast<double>(i), 1.8,
            std::string(planYokeLabels[i]) + " DEPTH: " + std::to_string(depth) + " mm",
            "ANNOTATION");
    }

    for (std::size_t phase = 0; phase < 3; ++phase) {
        const double x = limbX[phase];
        const double width = mm(core.limbWidths[phase].si());
        const double depth = mm(core.limbDepths[phase].si());
        dxf.rectangle(planX + x, planY, width, depth, "OUTLINE");
        const auto& bobbin = geometry.bobbins[phase];
        const auto& winding = geometry.windingPacks[phase];
        const double bodyW = mm(bobbin.innerWidth.si() + 2.0 * bobbin.wallThickness.si());
        const double bodyD = mm(bobbin.innerHeight.si() + 2.0 * bobbin.wallThickness.si());
        const double envelopeW = bodyW + 2.0 * mm(bobbin.windingClearance.si() + winding.radialBuild.si());
        const double envelopeD = bodyD + 2.0 * mm(bobbin.windingClearance.si() + winding.radialBuild.si());
        const double cx = x + 0.5 * width;
        const double cz = 0.5 * depth;
        dxf.rectangle(planX + cx - 0.5 * envelopeW, planY + cz - 0.5 * envelopeD,
            envelopeW, envelopeD, "WINDING_ENVELOPE");
    }
    dxf.text(planX, planY - 10.0, 2.5,
        "Maximum specified core stack depth: " + std::to_string(maxDepth) + " mm", "DIMENSIONS");
    dxf.text(planX, planY - 18.0, 2.2,
        "Limb stack depths A/B/C: " +
        std::to_string(mm(core.limbDepths[0].si())) + " / " +
        std::to_string(mm(core.limbDepths[1].si())) + " / " +
        std::to_string(mm(core.limbDepths[2].si())) + " mm", "DIMENSIONS");
    dxf.text(planX, planY - 26.0, 2.2,
        "Yoke stack depths T1/T2/B1/B2: " +
        std::to_string(mm(core.yokeDepths[0].si())) + " / " +
        std::to_string(mm(core.yokeDepths[1].si())) + " / " +
        std::to_string(mm(core.yokeDepths[2].si())) + " / " +
        std::to_string(mm(core.yokeDepths[3].si())) + " mm", "DIMENSIONS");

    // Side elevation: phase B is the central reference stack; all supplied
    // depth dimensions remain visible in the plan view above.
    const double sideX = 470.0, sideY = 285.0;
    dxf.text(sideX, sideY + totalHeight + 12.0, 3.5, "C - SIDE ELEVATION (PHASE B)", "ANNOTATION");
    const double depthB = mm(core.limbDepths[1].si());
    // Side projection separates the central limb from all four yoke members.
    // Each yoke keeps its own stack depth and thickness; no averaged depth is
    // substituted for the physical top/bottom members.
    verticalCoreMember(dxf, sideX, sideY + yokeBottom, depthB, limbHeight,
        gapIntervalsFor(core, domain::CoreSegment::LimbB));
    const std::array<double, 4> sideYokeDepths{
        mm(core.yokeDepths[0].si()), mm(core.yokeDepths[1].si()),
        mm(core.yokeDepths[2].si()), mm(core.yokeDepths[3].si())};
    const std::array<double, 4> sideYokeY{
        sideY + totalHeight - mm(core.yokeThicknesses[0].si()),
        sideY + totalHeight - mm(core.yokeThicknesses[1].si()),
        sideY, sideY};
    for (std::size_t i = 0; i < sideYokeDepths.size(); ++i) {
        dxf.rectangle(sideX, sideYokeY[i], sideYokeDepths[i],
            mm(core.yokeThicknesses[i].si()), "OUTLINE");
    }
    const auto& bobbinB = geometry.bobbins[1];
    const auto& windingB = geometry.windingPacks[1];
    const double bobbinYB = yokeBottom + 0.5 *
        (mm(std::min(core.windowHeights[0].si(), core.windowHeights[1].si())) - mm(bobbinB.axialLength.si()));
    const double bobbinDepthB = mm(bobbinB.innerHeight.si() + 2.0 * bobbinB.wallThickness.si());
    const double windingDepthB = bobbinDepthB +
        2.0 * mm(bobbinB.windingClearance.si() + windingB.radialBuild.si());
    const double windingYB = bobbinYB + 0.5 *
        (mm(bobbinB.axialLength.si()) - mm(windingB.axialHeight.si()));
    dxf.rectangle(sideX + 0.5 * (depthB - bobbinDepthB), sideY + bobbinYB,
        bobbinDepthB, mm(bobbinB.axialLength.si()), "OUTLINE");
    dxf.rectangle(sideX + 0.5 * (depthB - windingDepthB), sideY + windingYB,
        windingDepthB, mm(windingB.axialHeight.si()), "WINDING_ENVELOPE");
    dxf.verticalDimension(sideX + depthB, sideY + bobbinYB,
        sideY + bobbinYB + mm(bobbinB.axialLength.si()), sideX + depthB + 18.0,
        "BOBBIN B AXIAL LENGTH " + std::to_string(mm(bobbinB.axialLength.si())) + " mm");
    dxf.horizontalDimension(sideX + 0.5 * (depthB - bobbinDepthB),
        sideX + 0.5 * (depthB + bobbinDepthB), sideY + bobbinYB,
        sideY + bobbinYB - 15.0,
        "BOBBIN B DEPTH " + std::to_string(bobbinDepthB) + " mm");
    dxf.text(sideX, 165.0, 2.2,
        "Phase B winding envelope: axial " +
        std::to_string(mm(windingB.axialHeight.si())) + " mm; radial build " +
        std::to_string(mm(windingB.radialBuild.si())) + " mm; clearance " +
        std::to_string(mm(bobbinB.windingClearance.si())) + " mm", "DIMENSIONS");
    dxf.text(sideX, 157.0, 2.2,
        "Bobbin B opening: " + std::to_string(mm(bobbinB.innerWidth.si())) +
        " x " + std::to_string(mm(bobbinB.innerHeight.si())) +
        " mm; wall " + std::to_string(mm(bobbinB.wallThickness.si())) + " mm", "DIMENSIONS");

    // List the exact air-gap source records as a traceable schedule. The
    // front/plan outlines above also show the gaps in the projections where
    // their cut direction is visible.
    const auto segmentName = [](domain::CoreSegment segment) -> const char* {
        switch (segment) {
        case domain::CoreSegment::LimbA: return "LIMB A";
        case domain::CoreSegment::LimbB: return "LIMB B";
        case domain::CoreSegment::LimbC: return "LIMB C";
        case domain::CoreSegment::YokeTopAB: return "YOKE TOP AB";
        case domain::CoreSegment::YokeTopBC: return "YOKE TOP BC";
        case domain::CoreSegment::YokeBottomAB: return "YOKE BOTTOM AB";
        case domain::CoreSegment::YokeBottomBC: return "YOKE BOTTOM BC";
        }
        return "UNKNOWN";
    };
    double gapScheduleY = 112.0;
    dxf.text(470.0, gapScheduleY, 2.5,
        "AIR GAP SCHEDULE (" + std::to_string(core.airGaps.size()) + ")", "ANNOTATION");
    gapScheduleY -= 6.0;
    for (const auto& gap : core.airGaps) {
        dxf.text(470.0, gapScheduleY, 1.8,
            std::string(segmentName(gap.segment)) + " - position " +
            std::to_string(mm(gap.positionFromSegmentStart.si())) + " mm; length " +
            std::to_string(mm(gap.length.si())) + " mm", "ANNOTATION");
        gapScheduleY -= 4.5;
    }
    dxf.text(470.0, gapScheduleY - 2.0, 2.0,
        "Joint interfaces: " + std::to_string(core.joints.size()), "ANNOTATION");
    dxf.text(470.0, 72.0, 2.0,
        "Core and winding outlines are physical envelopes; no conductor detail, tolerances or BOM are implied.", "ANNOTATION");

    dxf.finish();
    dxf.stream.flush();
    if (!dxf.stream.good())
        return Result<void>::failure(drawingError(
            ErrorCode::SolverFailure, "DXF write failed before the file was completed.", "drawing.write"));
    return Result<void>::success();
}

} // namespace p135::cad
