#include "p135/cad/three_limb_core.hpp"

#include <BRep_Builder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Compound.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <GProp_GProps.hxx>

#include <algorithm>
#include <array>
#include <cmath>
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
    if (solids != 10)
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "The reactor assembly must retain seven core components and three bobbin solids.",
            "assembly");
    return Result<TopoDS_Shape>::success(assembly);
}

} // namespace p135::cad
