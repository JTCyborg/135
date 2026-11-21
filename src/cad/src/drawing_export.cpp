#include "p135/cad/drawing_export.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>

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

    void begin()
    {
        pair(0, "SECTION"); pair(2, "HEADER");
        pair(9, "$ACADVER"); pair(1, "AC1009");
        pair(9, "$INSUNITS"); pair(70, "4");
        pair(9, "$MEASUREMENT"); pair(70, "1");
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
    dxf.rectangle(frontX + xA, frontY + yokeBottom, wA, limbHeight, "OUTLINE");
    dxf.rectangle(frontX + xB, frontY + yokeBottom, wB, limbHeight, "OUTLINE");
    dxf.rectangle(frontX + xC, frontY + yokeBottom, wC, limbHeight, "OUTLINE");
    dxf.rectangle(frontX + xA, frontY + totalHeight - mm(core.yokeThicknesses[0].si()), wA + gapAB + 0.5 * wB,
        mm(core.yokeThicknesses[0].si()), "OUTLINE");
    dxf.rectangle(frontX + xB + 0.5 * wB, frontY + totalHeight - mm(core.yokeThicknesses[1].si()),
        0.5 * wB + gapBC + wC, mm(core.yokeThicknesses[1].si()), "OUTLINE");
    dxf.rectangle(frontX + xA, frontY, wA + gapAB + 0.5 * wB,
        mm(core.yokeThicknesses[2].si()), "OUTLINE");
    dxf.rectangle(frontX + xB + 0.5 * wB, frontY, 0.5 * wB + gapBC + wC,
        mm(core.yokeThicknesses[3].si()), "OUTLINE");

    const std::array<double, 3> limbX{xA, xB, xC};
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const auto& bobbin = geometry.bobbins[phase];
        const auto& winding = geometry.windingPacks[phase];
        const double bobbinLength = mm(bobbin.axialLength.si());
        const double windowHeight = mm(core.windowHeights[phase == 0 ? 0 : 1].si());
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
    dxf.text(frontX, frontY - 12.0, 2.5,
        "Core overall: " + std::to_string(totalWidth) + " x " + std::to_string(totalHeight) + " mm", "DIMENSIONS");

    // Plan view: core stack depths and the explicit bobbin/winding cross-sections.
    const double planX = 25.0, planY = 80.0;
    dxf.text(planX, planY + 115.0, 3.5, "B - PLAN VIEW (X-Z)", "ANNOTATION");
    const double maxDepth = std::max({mm(core.limbDepths[0].si()), mm(core.limbDepths[1].si()),
                                      mm(core.limbDepths[2].si()), mm(core.yokeDepths[0].si()),
                                      mm(core.yokeDepths[1].si()), mm(core.yokeDepths[2].si()),
                                      mm(core.yokeDepths[3].si())});
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

    // Side elevation: phase B is the central reference stack; all supplied
    // depth dimensions remain visible in the plan view above.
    const double sideX = 470.0, sideY = 285.0;
    dxf.text(sideX, sideY + totalHeight + 12.0, 3.5, "C - SIDE ELEVATION (PHASE B)", "ANNOTATION");
    const double depthB = mm(core.limbDepths[1].si());
    dxf.rectangle(sideX, sideY, depthB, totalHeight, "OUTLINE");
    const auto& bobbinB = geometry.bobbins[1];
    const auto& windingB = geometry.windingPacks[1];
    const double bobbinYB = yokeBottom + 0.5 *
        (mm(core.windowHeights[0].si()) - mm(bobbinB.axialLength.si()));
    const double bobbinDepthB = mm(bobbinB.innerHeight.si() + 2.0 * bobbinB.wallThickness.si());
    const double windingDepthB = bobbinDepthB +
        2.0 * mm(bobbinB.windingClearance.si() + windingB.radialBuild.si());
    const double windingYB = bobbinYB + 0.5 *
        (mm(bobbinB.axialLength.si()) - mm(windingB.axialHeight.si()));
    dxf.rectangle(sideX + 0.5 * (depthB - bobbinDepthB), sideY + bobbinYB,
        bobbinDepthB, mm(bobbinB.axialLength.si()), "OUTLINE");
    dxf.rectangle(sideX + 0.5 * (depthB - windingDepthB), sideY + windingYB,
        windingDepthB, mm(windingB.axialHeight.si()), "WINDING_ENVELOPE");

    // Explicit gap callouts are annotation-only; their exact cut positions are
    // retained in the source geometry and not inferred from this simplified view.
    dxf.text(470.0, 80.0, 2.5,
        "Air-gap records: " + std::to_string(core.airGaps.size()) +
        " | Joint interfaces: " + std::to_string(core.joints.size()), "ANNOTATION");
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
