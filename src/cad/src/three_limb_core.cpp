#include "p135/cad/three_limb_core.hpp"

#include <BRep_Builder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <vector>
#include <utility>

namespace p135::cad {
namespace {

template <class T>
Result<T> failure(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CAD_CORE_BUILD_FAILED", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<T>::failure(std::move(error));
}

Result<TopoDS_Compound> makeSegmentedBox(
    const gp_Pnt& origin, double lengthX, double lengthY, double depth,
    bool splitAlongX, const std::vector<std::pair<double, double>>& gaps,
    const std::string& field)
{
    constexpr double toMillimetres = 1000.0;
    BRep_Builder builder;
    TopoDS_Compound component;
    builder.MakeCompound(component);
    double cursor = 0.0;
    const auto addPiece = [&](double start, double end) -> bool {
        // Adjacent air-gap intervals may leave an empty material span
        // between them. Treat that span as a no-op rather than a failed solid.
        if (end < start - 1.0e-12) return false;
        if (end - start <= 1.0e-12) return true;
        const gp_Pnt point(
            (origin.X() + (splitAlongX ? start : 0.0)) * toMillimetres,
            (origin.Y() + (splitAlongX ? 0.0 : start)) * toMillimetres,
            origin.Z() * toMillimetres);
        const auto solid = BRepPrimAPI_MakeBox(
            point,
            (splitAlongX ? end - start : lengthX) * toMillimetres,
            (splitAlongX ? lengthY : end - start) * toMillimetres,
            depth * toMillimetres).Shape();
        if (solid.IsNull() || !BRepCheck_Analyzer(solid).IsValid()) return false;
        GProp_GProps volumeProperties;
        BRepGProp::VolumeProperties(solid, volumeProperties);
        const double volume = volumeProperties.Mass();
        if (!std::isfinite(volume) || volume <= 1.0e-12) return false;
        builder.Add(component, solid);
        return true;
    };
    for (const auto& gap : gaps) {
        if (!addPiece(cursor, gap.first))
            return failure<TopoDS_Compound>(ErrorCode::SolverFailure,
                "OCCT failed to create a core segment adjacent to an air gap.", field);
        cursor = gap.second;
    }
    const double totalLength = splitAlongX ? lengthX : lengthY;
    if (!addPiece(cursor, totalLength))
        return failure<TopoDS_Compound>(ErrorCode::SolverFailure,
            "OCCT failed to create the final core segment after an air gap.", field);
    return Result<TopoDS_Compound>::success(component);
}

} // namespace


Result<TopoDS_Shape> buildBobbinShape(const domain::BobbinPhysicalGeometry& geometry)
{
    const auto validation = domain::validate(geometry);
    if (!validation) return Result<TopoDS_Shape>::failure(validation.error());
    if (geometry.flangeThickness.si() <= 0.0)
        return failure<TopoDS_Shape>(ErrorCode::UnsupportedConfiguration,
            "A CAD bobbin requires a positive physical flange thickness.", "bobbin.flangeThickness");

    constexpr double mm = 1000.0;
    const double innerW = geometry.innerWidth.si();
    const double innerH = geometry.innerHeight.si();
    const double length = geometry.axialLength.si();
    const double wall = geometry.wallThickness.si();
    const double flangeT = geometry.flangeThickness.si();
    const double bodyW = innerW + 2.0 * wall;
    const double bodyH = innerH + 2.0 * wall;
    const double flangeW = geometry.flangeOuterWidth.si();
    const double flangeH = geometry.flangeOuterHeight.si();

    const gp_Pnt bodyOrigin(-0.5 * bodyW * mm, 0.0, -0.5 * bodyH * mm);
    const auto tube = BRepPrimAPI_MakeBox(bodyOrigin, bodyW * mm, length * mm, bodyH * mm).Shape();
    const auto lowerFlange = BRepPrimAPI_MakeBox(
        gp_Pnt(-0.5 * flangeW * mm, 0.0, -0.5 * flangeH * mm),
        flangeW * mm, flangeT * mm, flangeH * mm).Shape();
    const auto upperFlange = BRepPrimAPI_MakeBox(
        gp_Pnt(-0.5 * flangeW * mm, (length - flangeT) * mm, -0.5 * flangeH * mm),
        flangeW * mm, flangeT * mm, flangeH * mm).Shape();

    BRepAlgoAPI_Fuse fuseLower(tube, lowerFlange);
    fuseLower.Build();
    if (!fuseLower.IsDone() || fuseLower.Shape().IsNull())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to fuse the lower bobbin flange to the tube.", "bobbin.lowerFlange");
    BRepAlgoAPI_Fuse fuseUpper(fuseLower.Shape(), upperFlange);
    fuseUpper.Build();
    if (!fuseUpper.IsDone() || fuseUpper.Shape().IsNull())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to fuse the upper bobbin flange to the tube.", "bobbin.upperFlange");

    const auto bore = BRepPrimAPI_MakeBox(
        gp_Pnt(-0.5 * innerW * mm, -1.0e-3, -0.5 * innerH * mm),
        innerW * mm, (length + 2.0e-3) * mm, innerH * mm).Shape();
    BRepAlgoAPI_Cut hollow(fuseUpper.Shape(), bore);
    hollow.Build();
    if (!hollow.IsDone() || hollow.Shape().IsNull() ||
        !BRepCheck_Analyzer(hollow.Shape()).IsValid())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to create a valid hollow bobbin solid.", "bobbin");

    std::size_t solids = 0;
    for (TopExp_Explorer it(hollow.Shape(), TopAbs_SOLID); it.More(); it.Next()) ++solids;
    if (solids != 1)
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "The bobbin must be one connected solid after flange fusion and bore cutting.", "bobbin");

    GProp_GProps volumeProperties;
    BRepGProp::VolumeProperties(hollow.Shape(), volumeProperties);
    if (!std::isfinite(volumeProperties.Mass()) || volumeProperties.Mass() <= 0.0)
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "The bobbin solid has a non-positive or non-finite volume.", "bobbin");
    return Result<TopoDS_Shape>::success(hollow.Shape());
}

Result<TopoDS_Shape> buildThreeLimbCoreShape(const domain::ReactorPhysicalGeometry& geometry)
{
    const auto validation = domain::validate(geometry);
    if (!validation) return Result<TopoDS_Shape>::failure(validation.error());

    const auto& g = geometry.core;
    for (std::size_t i = 0; i < g.joints.size(); ++i) {
        if (g.joints[i].construction == domain::CoreJointType::StepLap) {
            Error error;
            error.code = ErrorCode::UnsupportedConfiguration;
            error.message = "Step-Lap physical packet geometry is validated but not yet supported by the OCCT builder.";
            error.diagnostics.push_back(Diagnostic{
                Severity::Error, "CAD_STEP_LAP_NOT_IMPLEMENTED", error.message,
                "core.joints[" + std::to_string(i) + "]", std::nullopt, std::nullopt
            });
            return Result<TopoDS_Shape>::failure(std::move(error));
        }
    }
    const double width = g.coreOverallWidth.si();
    const double height = g.coreOverallHeight.si();
    // Derive limb centers from the explicit physical limb and window widths.
    // Domain validation guarantees these stack-ups equal the declared overall
    // width and center spacings; no magnetic Ae/Aw/le values are used here.
    const double limbA = g.limbWidths[0].si();
    const double limbB = g.limbWidths[1].si();
    const double windowAB = g.windowWidths[0].si();
    const double windowBC = g.windowWidths[1].si();
    const double centerA = 0.5 * limbA;
    const double centerB = centerA + 0.5 * limbA + windowAB + 0.5 * limbB;
    const double centerC = centerB + 0.5 * limbB + windowBC + 0.5 * g.limbWidths[2].si();
    const std::array<double, 3> centers{centerA, centerB, centerC};

    const double bottomYoke = 0.5 * (g.yokeThicknesses[2].si() + g.yokeThicknesses[3].si());
    const double topYoke = 0.5 * (g.yokeThicknesses[0].si() + g.yokeThicknesses[1].si());
    const double limbBottom = bottomYoke;
    const double limbTop = height - topYoke;
    if (!std::isfinite(limbTop - limbBottom) || limbTop <= limbBottom)
        return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
            "Core height does not leave a positive limb height between yokes.", "core.coreOverallHeight");

    const auto gapsFor = [&](domain::CoreSegment segment) {
        std::vector<std::pair<double, double>> result;
        for (const auto& gap : g.airGaps) {
            if (gap.segment == segment) {
                const double start = gap.positionFromSegmentStart.si();
                result.push_back({start, start + gap.length.si()});
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    };

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    for (std::size_t i = 0; i < 3; ++i) {
        const double limbWidth = g.limbWidths[i].si();
        const double x = std::clamp(centers[i] - 0.5 * limbWidth, 0.0, width - limbWidth);
        const double depth = g.limbDepths[i].si();
        const auto segment = static_cast<domain::CoreSegment>(
            static_cast<std::size_t>(domain::CoreSegment::LimbA) + i);
        const auto limb = makeSegmentedBox(
            gp_Pnt(x, limbBottom, 0.0), limbWidth, limbTop - limbBottom, depth,
            false, gapsFor(segment), "core.limbs[" + std::to_string(i) + "]");
        if (!limb) return Result<TopoDS_Shape>::failure(limb.error());
        builder.Add(compound, limb.value());
    }

    // Segment order: top AB, top BC, bottom AB, bottom BC.
    const std::array<std::array<std::size_t, 2>, 4> adjacent{{
        {{0, 1}}, {{1, 2}}, {{0, 1}}, {{1, 2}}
    }};
    const std::array<domain::CoreSegment, 4> yokeSegments{{
        domain::CoreSegment::YokeTopAB, domain::CoreSegment::YokeTopBC,
        domain::CoreSegment::YokeBottomAB, domain::CoreSegment::YokeBottomBC
    }};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto a = adjacent[i][0];
        const auto b = adjacent[i][1];
        // Split each horizontal yoke at the centre plane of the middle
        // limb. AB owns the left half of B; BC owns the right half. This
        // avoids overlapping yoke solids while retaining both limb contacts.
        const bool isLeftHalf = (i % 2) == 0;
        const double x0 = isLeftHalf
            ? centers[a] - 0.5 * g.limbWidths[a].si()
            : centers[a];
        const double x1 = isLeftHalf
            ? centers[b]
            : centers[b] + 0.5 * g.limbWidths[b].si();
        const double yokeHeight = g.yokeThicknesses[i].si();
        const double y = i < 2 ? height - yokeHeight : 0.0;
        const double depth = g.yokeDepths[i].si();
        const auto yoke = makeSegmentedBox(
            gp_Pnt(x0, y, 0.0), x1 - x0, yokeHeight, depth,
            true, gapsFor(yokeSegments[i]), "core.yokeSegments[" + std::to_string(i) + "]");
        if (!yoke) return Result<TopoDS_Shape>::failure(yoke.error());
        builder.Add(compound, yoke.value());
    }

    // Each of the seven physical components is represented by a sub-compound.
    // A gap splits only its referenced component; no artificial material is
    // inserted into the gap and the component grouping remains explicit.
    if (compound.IsNull() || !BRepCheck_Analyzer(compound).IsValid())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to create a topologically valid core assembly compound.", "core");
    return Result<TopoDS_Shape>::success(compound);
}



Result<TopoDS_Shape> buildWindingPackEnvelopeShape(
    const domain::BobbinPhysicalGeometry& bobbin,
    const domain::WindingPackPhysicalGeometry& winding)
{
    const auto bobbinValidation = domain::validate(bobbin);
    if (!bobbinValidation) return Result<TopoDS_Shape>::failure(bobbinValidation.error());
    const auto windingValidation = domain::validate(winding);
    if (!windingValidation) return Result<TopoDS_Shape>::failure(windingValidation.error());

    const double toMm = 1000.0;
    const double bobbinBodyWidth = bobbin.innerWidth.si() + 2.0 * bobbin.wallThickness.si();
    const double bobbinBodyDepth = bobbin.innerHeight.si() + 2.0 * bobbin.wallThickness.si();
    const double clearance = bobbin.windingClearance.si();
    const double radialBuild = winding.radialBuild.si();
    const double innerWidth = bobbinBodyWidth + 2.0 * clearance;
    const double innerDepth = bobbinBodyDepth + 2.0 * clearance;
    const double outerWidth = innerWidth + 2.0 * radialBuild;
    const double outerDepth = innerDepth + 2.0 * radialBuild;
    const double axialHeight = winding.axialHeight.si();
    const double requiredAxialLength = axialHeight + 2.0 * winding.endClearance.si();
    const double axialTolerance = std::max(1.0e-9, bobbin.axialLength.si() * 1.0e-6);
    if (requiredAxialLength > bobbin.axialLength.si() + axialTolerance)
        return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
            "Winding axial height plus both end clearances must fit inside the bobbin.",
            "windingPack.axialHeight");

    const double xOuter = -0.5 * outerWidth * toMm;
    const double zOuter = -0.5 * outerDepth * toMm;
    const double xInner = -0.5 * innerWidth * toMm;
    const double zInner = -0.5 * innerDepth * toMm;
    BRepPrimAPI_MakeBox outerMaker(
        gp_Pnt(xOuter, 0.0, zOuter),
        outerWidth * toMm, axialHeight * toMm, outerDepth * toMm);
    // Extend the cutter slightly beyond both axial end faces. A cutter
    // exactly coplanar with the outer box end faces makes the Boolean
    // subtraction fragile in OCCT and can leave invalid/open topology.
    constexpr double cutOverrunMm = 1.0e-3;
    BRepPrimAPI_MakeBox innerMaker(
        gp_Pnt(xInner, -cutOverrunMm, zInner),
        innerWidth * toMm, axialHeight * toMm + 2.0 * cutOverrunMm,
        innerDepth * toMm);

    // Explicitly build both primitives before querying IsDone()/Shape().
    // This keeps the builder state deterministic across OCCT versions.
    outerMaker.Build();
    innerMaker.Build();
    if (!outerMaker.IsDone())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to build the outer winding-pack envelope box.",
            "windingPack.envelope.outer");
    if (!innerMaker.IsDone())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to build the inner winding-pack cutter box.",
            "windingPack.envelope.inner");

    const TopoDS_Shape outerShape = outerMaker.Shape();
    const TopoDS_Shape innerShape = innerMaker.Shape();
    if (outerShape.IsNull() || innerShape.IsNull())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT returned a null winding-pack envelope primitive.",
            "windingPack.envelope");

    BRepAlgoAPI_Cut cut(outerShape, innerShape);
    cut.Build();
    if (!cut.IsDone() || cut.Shape().IsNull() ||
        !BRepCheck_Analyzer(cut.Shape()).IsValid())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to create a valid hollow winding-pack envelope.",
            "windingPack.envelope");

    std::size_t solidCount = 0;
    for (TopExp_Explorer it(cut.Shape(), TopAbs_SOLID); it.More(); it.Next())
        ++solidCount;
    GProp_GProps volumeProperties;
    BRepGProp::VolumeProperties(cut.Shape(), volumeProperties);
    if (solidCount != 1 || !std::isfinite(volumeProperties.Mass()) ||
        volumeProperties.Mass() <= 0.0)
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "Winding-pack envelope must be one finite, positive-volume solid.",
            "windingPack.envelope");
    return Result<TopoDS_Shape>::success(cut.Shape());
}

static Result<TopoDS_Shape> buildDetailedWindingTurnsShapeImpl(
    const domain::BobbinPhysicalGeometry& bobbin,
    const domain::WindingPackPhysicalGeometry& winding)
{
    const auto bobbinValidation = domain::validate(bobbin);
    if (!bobbinValidation) return Result<TopoDS_Shape>::failure(bobbinValidation.error());
    const auto windingValidation = domain::validate(winding);
    if (!windingValidation) return Result<TopoDS_Shape>::failure(windingValidation.error());

    if (!winding.turns || !winding.turnsPerLayer || !winding.parallelConductors ||
        winding.sections != 1 ||
        (winding.conductorShape != "rectangular" && winding.conductorShape != "foil" &&
         winding.conductorShape != "round"))
        return failure<TopoDS_Shape>(ErrorCode::UnsupportedConfiguration,
            "Individual turn solids require explicit conductor dimensions and a single winding section.",
            "windingPack.conductorGeometry");

    constexpr double mm = 1000.0;
    if (winding.conductorShape == "round") {
        if (!winding.bareConductorDiameter || winding.bareConductorDiameter->si() <= 0.0)
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Round detailed winding requires a positive bare conductor diameter.",
                "windingPack.bareConductorDiameter");

        const double bareDiameter = winding.bareConductorDiameter->si();
        const double insulatedDiameter = winding.insulatedConductorDiameter
            ? winding.insulatedConductorDiameter->si()
            : bareDiameter + 2.0 * winding.conductorInsulationBuild.si();
        if (!std::isfinite(insulatedDiameter) || insulatedDiameter <= bareDiameter)
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Round conductor insulation must have a positive thickness outside the copper diameter.",
                "windingPack.insulatedConductorDiameter");

        const double copperRadius = 0.5 * bareDiameter * mm;
        const double insulatedRadius = 0.5 * insulatedDiameter * mm;
        const double bodyWidth = bobbin.innerWidth.si() + 2.0 * bobbin.wallThickness.si();
        const double bodyDepth = bobbin.innerHeight.si() + 2.0 * bobbin.wallThickness.si();
        const double radialPitch = winding.radialBuild.si() /
            static_cast<double>(winding.layerCount);
        const std::uint64_t conductorsPerLayer =
            static_cast<std::uint64_t>(*winding.turnsPerLayer) *
            static_cast<std::uint64_t>(*winding.parallelConductors);
        if (conductorsPerLayer == 0)
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Round winding has zero conductors per layer.", "windingPack.turnsPerLayer");
        const std::uint64_t configuredTurns =
            static_cast<std::uint64_t>(*winding.turnsPerLayer) *
            static_cast<std::uint64_t>(winding.layerCount);
        if (configuredTurns != static_cast<std::uint64_t>(*winding.turns))
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Total turns must equal turns per layer multiplied by the number of layers; refusing to silently reduce the winding turn count.",
                "windingPack.turns");

        const double axialPitch = (winding.axialHeight.si() - insulatedDiameter) /
            static_cast<double>(conductorsPerLayer + *winding.parallelConductors - 1ULL);
        const double tolerance = std::max(1.0e-9, radialPitch * 1.0e-6);
        if (axialPitch <= 0.0 ||
            insulatedDiameter + winding.layerInsulationBuild.si() > radialPitch + tolerance ||
            insulatedDiameter > axialPitch + std::max(1.0e-9, axialPitch * 1.0e-6))
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Insulated round conductor does not fit the specified radial or axial pitch.",
                "windingPack.conductorGeometry");

        BRep_Builder builder;
        TopoDS_Compound windingSolids;
        builder.MakeCompound(windingSolids);
        const double innerClearance =
            bobbin.windingClearance.si() + winding.mainInsulationBuild.si();
        const double axialStart = winding.endClearance.si();
        // Each conductor follows a continuous rounded-rectangle helix.
        // A single interpolated 3-D spline gives the corners a finite bend
        // radius instead of joining straight edges at sharp 90-degree turns.
        constexpr std::size_t sideSamples = 8;
        constexpr std::size_t cornerSamples = 12;
        constexpr double pi = 3.14159265358979323846;
        const double bendRadius = 1.5 * insulatedDiameter;
        const auto roundedPerimeter = [&](double halfX, double halfZ,
                                           double yStart, double yPitch) {
            std::vector<gp_Pnt> points;
            const double straightX = halfX - bendRadius;
            const double straightZ = halfZ - bendRadius;
            const double perimeter =
                4.0 * (straightX + straightZ) + 2.0 * pi * bendRadius;
            double travelled = 0.0;
            auto append = [&](double x, double z, double ds) {
                travelled += ds;
                points.emplace_back(x * mm,
                    (yStart + yPitch * travelled / perimeter) * mm,
                    z * mm);
            };
            points.emplace_back(-straightX * mm, yStart * mm, -halfZ * mm);
            auto line = [&](double x0, double z0, double x1, double z1) {
                const double length = std::hypot(x1 - x0, z1 - z0);
                for (std::size_t i = 1; i <= sideSamples; ++i) {
                    const double t = static_cast<double>(i) /
                        static_cast<double>(sideSamples);
                    append(x0 + (x1 - x0) * t, z0 + (z1 - z0) * t,
                        length / static_cast<double>(sideSamples));
                }
            };
            auto arc = [&](double cx, double cz, double startAngle) {
                const double arcLength = 0.5 * pi * bendRadius;
                for (std::size_t i = 1; i <= cornerSamples; ++i) {
                    const double angle = startAngle + 0.5 * pi *
                        static_cast<double>(i) / static_cast<double>(cornerSamples);
                    append(cx + bendRadius * std::cos(angle),
                        cz + bendRadius * std::sin(angle),
                        arcLength / static_cast<double>(cornerSamples));
                }
            };
            line(-straightX, -halfZ, straightX, -halfZ);
            arc(straightX, -straightZ, -0.5 * pi);
            line(halfX, -straightZ, halfX, straightZ);
            arc(straightX, straightZ, 0.0);
            line(straightX, halfZ, -straightX, halfZ);
            arc(-straightX, straightZ, 0.5 * pi);
            line(-halfX, straightZ, -halfX, -straightZ);
            arc(-straightX, -straightZ, pi);
            return points;
        };

        for (std::uint32_t layer = 0; layer < winding.layerCount; ++layer) {
            const double layerOffset = static_cast<double>(layer) * radialPitch;
            const double halfX = 0.5 * bodyWidth + innerClearance +
                layerOffset + 0.5 * insulatedDiameter;
            const double halfZ = 0.5 * bodyDepth + innerClearance +
                layerOffset + 0.5 * insulatedDiameter;
            if (bendRadius >= std::min(halfX, halfZ))
                return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                    "The insulated wire diameter leaves no room for the required corner bend radius.",
                    "windingPack.cornerBendRadius");

            for (std::uint32_t parallel = 0; parallel < *winding.parallelConductors; ++parallel) {
                const double firstY = axialStart + 0.5 * insulatedDiameter +
                    static_cast<double>(parallel) * axialPitch;
                const double turnsPitch =
                    axialPitch * static_cast<double>(*winding.parallelConductors);
                std::vector<gp_Pnt> helixPoints;
                for (std::uint32_t turn = 0; turn < *winding.turnsPerLayer; ++turn) {
                    const auto loop = roundedPerimeter(
                        halfX, halfZ,
                        firstY + static_cast<double>(turn) * turnsPitch,
                        turnsPitch);
                    if (helixPoints.empty())
                        helixPoints.insert(helixPoints.end(), loop.begin(), loop.end());
                    else
                        helixPoints.insert(helixPoints.end(), loop.begin() + 1, loop.end());
                }
                if (helixPoints.size() < 4)
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "Rounded helical winding path has insufficient interpolation points.",
                        "windingPack.helicalPath");

                Handle(TColgp_HArray1OfPnt) interpolationPoints =
                    new TColgp_HArray1OfPnt(1,
                        static_cast<Standard_Integer>(helixPoints.size()));
                for (std::size_t point = 0; point < helixPoints.size(); ++point)
                    interpolationPoints->SetValue(
                        static_cast<Standard_Integer>(point + 1), helixPoints[point]);
                GeomAPI_Interpolate interpolation(
                    interpolationPoints, Standard_False, 1.0e-6);
                interpolation.Perform();
                if (!interpolation.IsDone())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to interpolate the rounded four-sided helical path.",
                        "windingPack.helicalPath");
                BRepBuilderAPI_MakeEdge path(interpolation.Curve());
                if (!path.IsDone())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to create the rounded helical path edge.",
                        "windingPack.helicalPath");

                const double straightX = halfX - bendRadius;
                const double straightZ = halfZ - bendRadius;
                const double perimeter =
                    4.0 * (straightX + straightZ) + 2.0 * pi * bendRadius;
                const gp_Vec tangent(1.0, turnsPitch / perimeter, 0.0);
                const gp_Pnt profileOrigin(
                    -straightX * mm, firstY * mm, -halfZ * mm);
                const gp_Ax2 profileAxis(
                    profileOrigin, gp_Dir(tangent), gp_Dir(0.0, 0.0, 1.0));
                const auto makeCircleWire = [&](double radius) -> TopoDS_Wire {
                    const gp_Circ circle(profileAxis, radius);
                    auto edge = BRepBuilderAPI_MakeEdge(circle);
                    if (!edge.IsDone()) return TopoDS_Wire();
                    BRepBuilderAPI_MakeWire wire;
                    wire.Add(edge.Edge());
                    return wire.IsDone() ? wire.Wire() : TopoDS_Wire();
                };
                const TopoDS_Wire copperProfile = makeCircleWire(copperRadius);
                const TopoDS_Wire insulationProfile = makeCircleWire(insulatedRadius);
                if (copperProfile.IsNull() || insulationProfile.IsNull())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to create the circular copper/insulation sweep profiles.",
                        "windingPack.helicalProfile");

                BRepBuilderAPI_MakeWire pathWire(path.Edge());
                if (!pathWire.IsDone())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to wrap the interpolated spline into a sweep wire.",
                        "windingPack.helicalPath");

                // A swept wire produces only a lateral shell with open ends.
                // PipeShell must receive filled circular faces to cap the ends
                // and construct closed solids; the enamel annulus is then made
                // by subtracting the copper solid from the insulated outer solid.
                BRepBuilderAPI_MakeFace copperProfileFace(copperProfile);
                BRepBuilderAPI_MakeFace insulationProfileFace(insulationProfile);
                if (!copperProfileFace.IsDone() || !insulationProfileFace.IsDone())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to create filled circular faces for the copper/insulation sweep.",
                        "windingPack.helicalProfile");

                BRepOffsetAPI_MakePipeShell copperSweep(pathWire.Wire());
                copperSweep.SetMode(false); // corrected Frenet trihedron
                copperSweep.Add(copperProfileFace.Face(), false, false);
                copperSweep.Build();
                const bool copperSolidBuilt = copperSweep.IsDone() && copperSweep.MakeSolid();

                BRepOffsetAPI_MakePipeShell insulationSweep(pathWire.Wire());
                insulationSweep.SetMode(false); // corrected Frenet trihedron
                insulationSweep.Add(insulationProfileFace.Face(), false, false);
                insulationSweep.Build();
                const bool insulationSolidBuilt =
                    insulationSweep.IsDone() && insulationSweep.MakeSolid();

                if (!copperSolidBuilt || !insulationSolidBuilt ||
                    copperSweep.Shape().IsNull() || insulationSweep.Shape().IsNull())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT pipe-shell sweep could not form closed copper and insulation solids.",
                        "windingPack.helicalSweep");

                BRepAlgoAPI_Cut insulationCut(insulationSweep.Shape(), copperSweep.Shape());
                insulationCut.Build();
                if (!insulationCut.IsDone() || insulationCut.Shape().IsNull() ||
                    !BRepCheck_Analyzer(copperSweep.Shape()).IsValid() ||
                    !BRepCheck_Analyzer(insulationCut.Shape()).IsValid())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to form a valid annular enamel solid around the copper helix.",
                        "windingPack.helicalSweep");

                std::size_t copperSolids = 0, insulationSolids = 0;
                for (TopExp_Explorer it(copperSweep.Shape(), TopAbs_SOLID); it.More(); it.Next()) ++copperSolids;
                for (TopExp_Explorer it(insulationCut.Shape(), TopAbs_SOLID); it.More(); it.Next()) ++insulationSolids;
                if (copperSolids != 1 || insulationSolids != 1)
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "Each rounded helical path must produce one copper solid and one separate insulation solid.",
                        "windingPack.helicalSolids");

                // Audit the swept section over the complete path (all four
                // straight sides and all four rounded corners). A constant
                // circular/annular profile must preserve its analytical area
                // times the measured centreline length.
                GProp_GProps pathLengthProperties;
                GProp_GProps copperVolumeProperties;
                GProp_GProps insulationVolumeProperties;
                BRepGProp::LinearProperties(pathWire.Wire(), pathLengthProperties);
                BRepGProp::VolumeProperties(copperSweep.Shape(), copperVolumeProperties);
                BRepGProp::VolumeProperties(insulationCut.Shape(), insulationVolumeProperties);
                const double pathLength = pathLengthProperties.Mass();
                const double expectedCopperVolume =
                    pi * copperRadius * copperRadius * pathLength;
                const double expectedInsulationVolume =
                    pi * (insulatedRadius * insulatedRadius -
                          copperRadius * copperRadius) * pathLength;
                const auto sectionMatches = [](double actual, double expected) {
                    return std::isfinite(actual) && std::isfinite(expected) &&
                        expected > 0.0 &&
                        std::abs(actual - expected) <= std::max(1.0e-7, expected * 2.0e-3);
                };
                if (!sectionMatches(copperVolumeProperties.Mass(), expectedCopperVolume) ||
                    !sectionMatches(insulationVolumeProperties.Mass(), expectedInsulationVolume))
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "The swept conductor/insulation cross-section changed along a phase winding path; diameter audit failed.",
                        "windingPack.conductorDiameterAudit");

                builder.Add(windingSolids, copperSweep.Shape());
                builder.Add(windingSolids, insulationCut.Shape());
            }
        }

        const std::uint64_t expected = 2ULL *
            static_cast<std::uint64_t>(winding.layerCount) *
            static_cast<std::uint64_t>(*winding.parallelConductors);
        std::uint64_t actual = 0;
        for (TopExp_Explorer it(windingSolids, TopAbs_SOLID); it.More(); it.Next()) ++actual;
        if (actual != expected || !BRepCheck_Analyzer(windingSolids).IsValid())
            return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                "Round helical winding must contain separate copper and insulation solids for every layer/parallel path.",
                "windingPack.helicalSolids");
        return Result<TopoDS_Shape>::success(windingSolids);
    }
    constexpr double cutOverrunMm = 1.0e-3;
    const double bodyWidth = bobbin.innerWidth.si() + 2.0 * bobbin.wallThickness.si();
    const double bodyDepth = bobbin.innerHeight.si() + 2.0 * bobbin.wallThickness.si();
    const double radialPitch = winding.radialBuild.si() /
        static_cast<double>(winding.layerCount);
    const double radialConductor = winding.bareConductorWidth->si() +
        2.0 * winding.conductorInsulationBuild.si();
    const double axialConductor = winding.bareConductorThickness->si() +
        2.0 * winding.conductorInsulationBuild.si();
    const std::uint64_t conductorsPerLayer =
        static_cast<std::uint64_t>(*winding.turnsPerLayer) *
        static_cast<std::uint64_t>(*winding.parallelConductors);
    if (conductorsPerLayer == 0 ||
        conductorsPerLayer > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
            "Winding conductor count exceeds the supported CAD iteration range.",
            "windingPack.parallelConductors");
    const double axialPitch = winding.axialHeight.si() /
        static_cast<double>(conductorsPerLayer);
    const double tolerance = std::max(1.0e-9, radialPitch * 1.0e-6);
    if (radialConductor + winding.layerInsulationBuild.si() > radialPitch + tolerance)
        return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
            "Insulated radial conductor width plus layer insulation exceeds the radial pitch.",
            "windingPack.radialBuild");
    if (axialConductor > axialPitch + std::max(1.0e-9, axialPitch * 1.0e-6))
        return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
            "Insulated axial conductor thickness does not fit the turn/parallel-conductor pitch.",
            "windingPack.axialHeight");

    BRep_Builder builder;
    TopoDS_Compound turns;
    builder.MakeCompound(turns);
    const double innerClearance = bobbin.windingClearance.si() +
        winding.mainInsulationBuild.si();
    const double axialStart = winding.endClearance.si();
    for (std::uint32_t layer = 0; layer < winding.layerCount; ++layer) {
        const double layerOffset = static_cast<double>(layer) * radialPitch;
        const double outerWidth = bodyWidth + 2.0 *
            (innerClearance + layerOffset + radialPitch);
        const double outerDepth = bodyDepth + 2.0 *
            (innerClearance + layerOffset + radialPitch);
        const double innerWidth = outerWidth - 2.0 * radialConductor;
        const double innerDepth = outerDepth - 2.0 * radialConductor;
        if (innerWidth <= 0.0 || innerDepth <= 0.0)
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Conductor radial build collapses the rectangular turn opening.",
                "windingPack.conductorCrossSection");

        for (std::uint32_t turn = 0; turn < *winding.turnsPerLayer; ++turn) {
            for (std::uint32_t parallel = 0; parallel < *winding.parallelConductors; ++parallel) {
                const std::uint64_t axialIndex =
                    static_cast<std::uint64_t>(turn) * *winding.parallelConductors + parallel;
                const double y = (axialStart +
                    (static_cast<double>(axialIndex) + 0.5) * axialPitch -
                    0.5 * axialConductor) * mm;
                const double xOuter = -0.5 * outerWidth * mm;
                const double zOuter = -0.5 * outerDepth * mm;
                const double xInner = -0.5 * innerWidth * mm;
                const double zInner = -0.5 * innerDepth * mm;
                BRepPrimAPI_MakeBox outerMaker(
                    gp_Pnt(xOuter, y, zOuter),
                    outerWidth * mm, axialConductor * mm, outerDepth * mm);
                BRepPrimAPI_MakeBox innerMaker(
                    gp_Pnt(xInner, y - cutOverrunMm, zInner),
                    innerWidth * mm, (axialConductor * mm) + 2.0 * cutOverrunMm,
                    innerDepth * mm);
                outerMaker.Build();
                innerMaker.Build();
                if (!outerMaker.IsDone() || !innerMaker.IsDone())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to create a conductor-turn ring primitive.",
                        "windingPack.turns");
                BRepAlgoAPI_Cut cut(outerMaker.Shape(), innerMaker.Shape());
                cut.Build();
                if (!cut.IsDone() || cut.Shape().IsNull() ||
                    !BRepCheck_Analyzer(cut.Shape()).IsValid())
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "OCCT failed to create a valid independent conductor-turn solid.",
                        "windingPack.turns");
                std::size_t solids = 0;
                for (TopExp_Explorer it(cut.Shape(), TopAbs_SOLID); it.More(); it.Next()) ++solids;
                if (solids != 1)
                    return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                        "Each detailed conductor turn must be one connected solid.",
                        "windingPack.turns");
                builder.Add(turns, cut.Shape());
            }
        }
    }
    const std::uint64_t expected = static_cast<std::uint64_t>(*winding.turns) *
        static_cast<std::uint64_t>(*winding.parallelConductors);
    std::uint64_t actual = 0;
    for (TopExp_Explorer it(turns, TopAbs_SOLID); it.More(); it.Next()) ++actual;
    if (actual != expected || !BRepCheck_Analyzer(turns).IsValid())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "Detailed winding CAD must contain exactly turns multiplied by parallel conductors independent solids.",
            "windingPack.turns");
    return Result<TopoDS_Shape>::success(turns);
}

Result<TopoDS_Shape> buildDetailedWindingTurnsShape(
    const domain::BobbinPhysicalGeometry& bobbin,
    const domain::WindingPackPhysicalGeometry& winding)
{
    try {
        return buildDetailedWindingTurnsShapeImpl(bobbin, winding);
    } catch (const Standard_Failure& exception) {
        const char* message = exception.what();
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            std::string("OCCT exception while building detailed winding: ") +
                (message != nullptr ? message : "unspecified Standard_Failure"),
            "windingPack.occtException");
    } catch (const std::exception& exception) {
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            std::string("C++ exception while building detailed winding: ") + exception.what(),
            "windingPack.cppException");
    } catch (...) {
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "Unknown exception while building detailed winding.",
            "windingPack.unknownException");
    }
}

Result<TopoDS_Shape> buildThreeLimbReactorAssemblyShape(
    const domain::ReactorPhysicalGeometry& geometry)
{
    const auto validation = domain::validate(geometry);
    if (!validation) return Result<TopoDS_Shape>::failure(validation.error());

    const auto core = buildThreeLimbCoreShape(geometry);
    if (!core) return Result<TopoDS_Shape>::failure(core.error());

    const auto& g = geometry.core;
    const double limbA = g.limbWidths[0].si();
    const double limbB = g.limbWidths[1].si();
    const double windowAB = g.windowWidths[0].si();
    const double windowBC = g.windowWidths[1].si();
    const double centerA = 0.5 * limbA;
    const double centerB = centerA + 0.5 * limbA + windowAB + 0.5 * limbB;
    const double centerC = centerB + 0.5 * limbB + windowBC + 0.5 * g.limbWidths[2].si();
    const std::array<double, 3> limbCentersX{centerA, centerB, centerC};

    const double bottomYoke = 0.5 *
        (g.yokeThicknesses[2].si() + g.yokeThicknesses[3].si());
    const std::array<double, 3> windowHeights{
        g.windowHeights[0].si(),
        std::min(g.windowHeights[0].si(), g.windowHeights[1].si()),
        g.windowHeights[1].si()
    };

    std::size_t expectedCoreSolids = 0;
    for (TopExp_Explorer it(core.value(), TopAbs_SOLID); it.More(); it.Next())
        ++expectedCoreSolids;

    BRep_Builder builder;
    TopoDS_Compound assembly;
    builder.MakeCompound(assembly);
    builder.Add(assembly, core.value());

    for (std::size_t i = 0; i < geometry.bobbins.size(); ++i) {
        const auto bobbin = buildBobbinShape(geometry.bobbins[i]);
        if (!bobbin) return Result<TopoDS_Shape>::failure(bobbin.error());

        const double bobbinLength = geometry.bobbins[i].axialLength.si();
        const double y = bottomYoke + 0.5 * (windowHeights[i] - bobbinLength);
        const double z = 0.5 * g.limbDepths[i].si();

        gp_Trsf placement;
        placement.SetTranslation(gp_Vec(
            limbCentersX[i] * 1000.0,
            y * 1000.0,
            z * 1000.0));
        BRepBuilderAPI_Transform located(bobbin.value(), placement, true);
        if (!located.IsDone() || located.Shape().IsNull())
            return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                "OCCT failed to position a bobbin on its core limb.",
                "bobbins[" + std::to_string(i) + "].placement");
        builder.Add(assembly, located.Shape());
    }

    if (assembly.IsNull() || !BRepCheck_Analyzer(assembly).IsValid())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to create a valid core-and-bobbin reactor assembly.",
            "assembly");
    std::size_t solids = 0;
    for (TopExp_Explorer it(assembly, TopAbs_SOLID); it.More(); it.Next()) ++solids;
    if (solids != expectedCoreSolids + geometry.bobbins.size())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "The reactor assembly must retain every core solid and all three bobbin solids.",
            "assembly");
    return Result<TopoDS_Shape>::success(assembly);
}

Result<TopoDS_Shape> buildThreeLimbReactorAssemblyWithWindingEnvelopesShape(
    const domain::ReactorPhysicalGeometry& geometry)
{
    const auto validation = domain::validate(geometry);
    if (!validation) return Result<TopoDS_Shape>::failure(validation.error());

    const auto core = buildThreeLimbCoreShape(geometry);
    if (!core) return Result<TopoDS_Shape>::failure(core.error());

    const auto& g = geometry.core;
    const double limbA = g.limbWidths[0].si();
    const double limbB = g.limbWidths[1].si();
    const double windowAB = g.windowWidths[0].si();
    const double windowBC = g.windowWidths[1].si();
    const std::array<double, 3> limbCentersX{
        0.5 * limbA,
        limbA + windowAB + 0.5 * limbB,
        limbA + windowAB + limbB + windowBC + 0.5 * g.limbWidths[2].si()
    };
    const double bottomYoke = 0.5 *
        (g.yokeThicknesses[2].si() + g.yokeThicknesses[3].si());
    const std::array<double, 3> windowHeights{
        g.windowHeights[0].si(),
        std::min(g.windowHeights[0].si(), g.windowHeights[1].si()),
        g.windowHeights[1].si()
    };

    std::size_t expectedCoreSolids = 0;
    for (TopExp_Explorer it(core.value(), TopAbs_SOLID); it.More(); it.Next())
        ++expectedCoreSolids;

    BRep_Builder builder;
    TopoDS_Compound assembly;
    builder.MakeCompound(assembly);
    builder.Add(assembly, core.value());

    for (std::size_t i = 0; i < geometry.bobbins.size(); ++i) {
        const auto& bobbinGeometry = geometry.bobbins[i];
        const auto& windingGeometry = geometry.windingPacks[i];
        const double flangeThickness = bobbinGeometry.flangeThickness.si();
        const double endClearance = windingGeometry.endClearance.si();
        const double axialHeight = windingGeometry.axialHeight.si();
        const double availableAxialLength =
            bobbinGeometry.axialLength.si() - 2.0 * (flangeThickness + endClearance);
        const double tolerance = std::max(1.0e-9, bobbinGeometry.axialLength.si() * 1.0e-6);
        if (availableAxialLength <= 0.0 || axialHeight > availableAxialLength + tolerance)
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Winding pack must fit between both bobbin flanges with the specified end clearances.",
                "windingPacks[" + std::to_string(i) + "].axialHeight");

        const auto bobbin = buildBobbinShape(bobbinGeometry);
        if (!bobbin) return Result<TopoDS_Shape>::failure(bobbin.error());
        const bool hasDetailedConductor =
            windingGeometry.turns && windingGeometry.turnsPerLayer &&
            windingGeometry.parallelConductors &&
            windingGeometry.sections == 1 &&
            (windingGeometry.conductorShape == "rectangular" ||
             windingGeometry.conductorShape == "foil" ||
             windingGeometry.conductorShape == "round");
        const auto winding = hasDetailedConductor
            ? buildDetailedWindingTurnsShape(bobbinGeometry, windingGeometry)
            : buildWindingPackEnvelopeShape(bobbinGeometry, windingGeometry);
        if (!winding) return Result<TopoDS_Shape>::failure(winding.error());

        const double bobbinLength = bobbinGeometry.axialLength.si();
        const double bobbinY = bottomYoke + 0.5 * (windowHeights[i] - bobbinLength);
        const double z = 0.5 * g.limbDepths[i].si();
        const double x = limbCentersX[i];
        const double windingY = bobbinY + flangeThickness + endClearance;

        gp_Trsf bobbinPlacement;
        bobbinPlacement.SetTranslation(gp_Vec(x * 1000.0, bobbinY * 1000.0, z * 1000.0));
        BRepBuilderAPI_Transform locatedBobbin(bobbin.value(), bobbinPlacement, true);
        if (!locatedBobbin.IsDone() || locatedBobbin.Shape().IsNull())
            return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                "OCCT failed to position a bobbin on its core limb.",
                "bobbins[" + std::to_string(i) + "].placement");

        gp_Trsf windingPlacement;
        windingPlacement.SetTranslation(gp_Vec(x * 1000.0, windingY * 1000.0, z * 1000.0));
        BRepBuilderAPI_Transform locatedWinding(winding.value(), windingPlacement, true);
        if (!locatedWinding.IsDone() || locatedWinding.Shape().IsNull())
            return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                "OCCT failed to position a winding-pack envelope around its bobbin.",
                "windingPacks[" + std::to_string(i) + "].placement");

        builder.Add(assembly, locatedBobbin.Shape());
        builder.Add(assembly, locatedWinding.Shape());
    }

    if (assembly.IsNull() || !BRepCheck_Analyzer(assembly).IsValid())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT failed to create a valid core, bobbin and winding-envelope assembly.",
            "assembly");

    std::size_t expectedSolidCount = expectedCoreSolids;
    std::array<std::size_t, 3> windingSolidCounts{};
    for (std::size_t phase = 0; phase < geometry.windingPacks.size(); ++phase) {
        const auto& winding = geometry.windingPacks[phase];
        const bool detailed =
            winding.turns && winding.parallelConductors &&
            winding.turnsPerLayer && winding.sections == 1 &&
            (winding.conductorShape == "rectangular" || winding.conductorShape == "foil" ||
             winding.conductorShape == "round");
        windingSolidCounts[phase] = !detailed ? 1U :
            (winding.conductorShape == "round"
                ? 2U * static_cast<std::size_t>(winding.layerCount) *
                    static_cast<std::size_t>(*winding.parallelConductors)
                : static_cast<std::size_t>(*winding.turns) *
                    static_cast<std::size_t>(*winding.parallelConductors));
        expectedSolidCount += 1U + windingSolidCounts[phase]; // bobbin + conductors/envelope
    }
    std::vector<TopoDS_Shape> members;
    for (TopExp_Explorer it(assembly, TopAbs_SOLID); it.More(); it.Next())
        members.push_back(it.Current());
    if (members.size() != expectedSolidCount)
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "The assembly solid count must match its core, bobbins and explicitly modelled winding conductors.",
            "assembly");

    // Reject unintended volumetric overlap. Turns within one winding are
    // generated from a deterministic non-overlapping pitch; checking every
    // pair of hundreds of turns with Boolean Common is prohibitively costly.
    // The validated bobbin opening contains the core limb, and the conductor
    // loops are outside the bobbin body by construction. Check conductors
    // against bobbins and other phases; check all remaining component pairs.
    std::vector<int> windingGroup(members.size(), -1);
    std::size_t memberCursor = expectedCoreSolids;
    for (std::size_t phase = 0; phase < windingSolidCounts.size(); ++phase) {
        ++memberCursor; // skip this phase's bobbin
        for (std::size_t turn = 0; turn < windingSolidCounts[phase]; ++turn)
            windingGroup[memberCursor++] = static_cast<int>(phase);
    }
    std::vector<Bnd_Box> memberBounds(members.size());
    for (std::size_t i = 0; i < members.size(); ++i)
        BRepBndLib::Add(members[i], memberBounds[i]);
    const auto boundsOverlap = [](const Bnd_Box& left, const Bnd_Box& right) {
        double lx0 = 0.0, ly0 = 0.0, lz0 = 0.0, lx1 = 0.0, ly1 = 0.0, lz1 = 0.0;
        double rx0 = 0.0, ry0 = 0.0, rz0 = 0.0, rx1 = 0.0, ry1 = 0.0, rz1 = 0.0;
        left.Get(lx0, ly0, lz0, lx1, ly1, lz1);
        right.Get(rx0, ry0, rz0, rx1, ry1, rz1);
        constexpr double toleranceMm = 1.0e-6;
        return lx0 <= rx1 + toleranceMm && rx0 <= lx1 + toleranceMm &&
            ly0 <= ry1 + toleranceMm && ry0 <= ly1 + toleranceMm &&
            lz0 <= rz1 + toleranceMm && rz0 <= lz1 + toleranceMm;
    };
    for (std::size_t i = 0; i < members.size(); ++i) {
        for (std::size_t j = i + 1; j < members.size(); ++j) {
            const bool sameWinding = windingGroup[i] >= 0 &&
                windingGroup[i] == windingGroup[j];
            const bool conductorAgainstCore =
                (windingGroup[i] >= 0 && j < expectedCoreSolids) ||
                (windingGroup[j] >= 0 && i < expectedCoreSolids);
            if (sameWinding || conductorAgainstCore)
                continue;
            if (!boundsOverlap(memberBounds[i], memberBounds[j]))
                continue;
            BRepAlgoAPI_Common common(members[i], members[j]);
            common.Build();
            if (!common.IsDone())
                return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                    "OCCT failed while checking pairwise intersections in the reactor assembly.",
                    "assembly.intersections");
            GProp_GProps commonVolume;
            BRepGProp::VolumeProperties(common.Shape(), commonVolume);
            const double volume = commonVolume.Mass();
            if (!std::isfinite(volume) || std::abs(volume) > 1.0e-8)
                return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                    "Reactor assembly members have an unintended volumetric intersection.",
                    "assembly.intersections");
        }
    }
    return Result<TopoDS_Shape>::success(assembly);
}

} // namespace p135::cad
