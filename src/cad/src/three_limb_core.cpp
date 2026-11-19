#include "p135/cad/three_limb_core.hpp"

#include <BRep_Builder.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
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

} // namespace

Result<TopoDS_Shape> buildThreeLimbCoreShape(const domain::ReactorPhysicalGeometry& geometry)
{
    const auto validation = domain::validate(geometry);
    if (!validation) return Result<TopoDS_Shape>::failure(validation.error());

    const auto& g = geometry.core;
    const double width = g.coreOverallWidth.si();
    const double height = g.coreOverallHeight.si();
    const double spacingAB = g.limbCenterSpacings[0].si();
    const double spacingBC = g.limbCenterSpacings[1].si();
    const double centerA = (width - spacingAB - spacingBC) * 0.5;
    const double centerB = centerA + spacingAB;
    const double centerC = centerB + spacingBC;
    const std::array<double, 3> centers{centerA, centerB, centerC};

    const double bottomYoke = 0.5 * (g.yokeThicknesses[2].si() + g.yokeThicknesses[3].si());
    const double topYoke = 0.5 * (g.yokeThicknesses[0].si() + g.yokeThicknesses[1].si());
    const double limbBottom = bottomYoke;
    const double limbTop = height - topYoke;
    if (!std::isfinite(limbTop - limbBottom) || limbTop <= limbBottom)
        return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
            "Core height does not leave a positive limb height between yokes.", "core.coreOverallHeight");

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    // Coordinates are metres converted to millimetres at the OCCT boundary.
    constexpr double toMillimetres = 1000.0;
    for (std::size_t i = 0; i < 3; ++i) {
        const double limbWidth = g.limbWidths[i].si();
        const double x = centers[i] - 0.5 * limbWidth;
        const double depth = g.limbDepths[i].si();
        if (x < 0.0 || x + limbWidth > width)
            return failure<TopoDS_Shape>(ErrorCode::InvalidInput,
                "Limb lies outside the declared overall core width.",
                "core.limbWidths[" + std::to_string(i) + "]");
        const auto limb = BRepPrimAPI_MakeBox(
            gp_Pnt(x * toMillimetres, limbBottom * toMillimetres, 0.0),
            limbWidth * toMillimetres, (limbTop - limbBottom) * toMillimetres,
            depth * toMillimetres).Shape();
        if (limb.IsNull() || !BRepCheck_Analyzer(limb).IsValid())
            return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                "OCCT generated an invalid limb solid.", "core.limbs[" + std::to_string(i) + "]");
        builder.Add(compound, limb);
    }

    // Segment order: top AB, top BC, bottom AB, bottom BC.
    const std::array<std::array<std::size_t, 2>, 4> adjacent{{
        {{0, 1}}, {{1, 2}}, {{0, 1}}, {{1, 2}}
    }};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto a = adjacent[i][0];
        const auto b = adjacent[i][1];
        const double x0 = centers[a] - 0.5 * g.limbWidths[a].si();
        const double x1 = centers[b] + 0.5 * g.limbWidths[b].si();
        const double yokeHeight = g.yokeThicknesses[i].si();
        const double y = i < 2 ? height - yokeHeight : 0.0;
        const double depth = g.yokeDepths[i].si();
        const auto yoke = BRepPrimAPI_MakeBox(
            gp_Pnt(x0 * toMillimetres, y * toMillimetres, 0.0),
            (x1 - x0) * toMillimetres, yokeHeight * toMillimetres,
            depth * toMillimetres).Shape();
        if (yoke.IsNull() || !BRepCheck_Analyzer(yoke).IsValid())
            return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
                "OCCT generated an invalid yoke segment.", "core.yokeSegments[" + std::to_string(i) + "]");
        builder.Add(compound, yoke);
    }

    if (compound.IsNull() || !BRepCheck_Analyzer(compound).IsValid())
        return failure<TopoDS_Shape>(ErrorCode::SolverFailure,
            "OCCT core compound failed topology validation.", "core");
    return Result<TopoDS_Shape>::success(compound);
}

} // namespace p135::cad
