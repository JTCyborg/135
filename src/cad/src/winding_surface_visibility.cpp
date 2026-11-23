#include "p135/cad/winding_surface_visibility.hpp"

#include "p135/cad/winding_surface_geometry.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <TopAbs_State.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
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
        Severity::Error, "CAD_WINDING_FACE_VISIBILITY_FAILED", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<T>::failure(std::move(error));
}

bool finiteShapeBounds(const TopoDS_Shape& shape, Bnd_Box& bounds)
{
    if (shape.IsNull() || !BRepCheck_Analyzer(shape).IsValid()) return false;
    BRepBndLib::Add(shape, bounds);
    return !bounds.IsVoid() && !bounds.IsOpenXmin() && !bounds.IsOpenXmax() &&
           !bounds.IsOpenYmin() && !bounds.IsOpenYmax() &&
           !bounds.IsOpenZmin() && !bounds.IsOpenZmax();
}

} // namespace

Result<std::vector<WindingFaceVisibilityResult>>
analyzeWindingEnvelopeFaceVisibility(
    const TopoDS_Shape& windingEnvelope,
    const std::vector<TopoDS_Shape>& occluders,
    const WindingFaceVisibilityOptions& options)
{
    if (windingEnvelope.IsNull() || !BRepCheck_Analyzer(windingEnvelope).IsValid())
        return failure<std::vector<WindingFaceVisibilityResult>>(
            ErrorCode::InvalidInput, "Winding envelope is null or invalid.", "windingEnvelope");
    if (options.samplesPerDirection < 2 || options.samplesPerDirection > 101 ||
        !std::isfinite(options.rayStartOffsetMm) || options.rayStartOffsetMm <= 0.0 ||
        !std::isfinite(options.geometricToleranceMm) || options.geometricToleranceMm <= 0.0)
        return failure<std::vector<WindingFaceVisibilityResult>>(
            ErrorCode::InvalidInput, "Visibility sampling options are outside supported bounds.", "options");

    Bnd_Box totalBounds;
    if (!finiteShapeBounds(windingEnvelope, totalBounds))
        return failure<std::vector<WindingFaceVisibilityResult>>(
            ErrorCode::InvalidInput, "Winding envelope has invalid or unbounded geometry.", "windingEnvelope");
    for (std::size_t i = 0; i < occluders.size(); ++i) {
        Bnd_Box bounds;
        if (!finiteShapeBounds(occluders[i], bounds))
            return failure<std::vector<WindingFaceVisibilityResult>>(
                ErrorCode::InvalidInput, "An occluder is null, invalid or unbounded.",
                "occluders[" + std::to_string(i) + "]");
        totalBounds.Add(bounds);
    }

    double x0 = 0.0, y0 = 0.0, z0 = 0.0, x1 = 0.0, y1 = 0.0, z1 = 0.0;
    totalBounds.Get(x0, y0, z0, x1, y1, z1);
    const double rayLength = std::sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0) + (z1-z0)*(z1-z0))
                           + options.rayStartOffsetMm;
    if (!std::isfinite(rayLength) || rayLength <= options.rayStartOffsetMm)
        return failure<std::vector<WindingFaceVisibilityResult>>(
            ErrorCode::SolverFailure, "Could not derive a finite occlusion ray length.", "geometry.bounds");

    std::vector<IntCurvesFace_ShapeIntersector> intersectors;
    intersectors.reserve(occluders.size());
    for (const auto& occluder : occluders) {
        intersectors.emplace_back();
        intersectors.back().Load(occluder, options.geometricToleranceMm);
    }

    const auto descriptors = extractWindingEnvelopeFaceGeometry(windingEnvelope);
    if (!descriptors)
        return Result<std::vector<WindingFaceVisibilityResult>>::failure(descriptors.error());

    std::vector<WindingFaceVisibilityResult> results;
    std::size_t faceIndex = 0;
    for (TopExp_Explorer it(windingEnvelope, TopAbs_FACE); it.More(); it.Next(), ++faceIndex) {
        const TopoDS_Face face = TopoDS::Face(it.Current());
        WindingFaceVisibilityResult result;
        result.faceIndex = faceIndex;

        BRepAdaptor_Surface surface(face, true);
        if (surface.GetType() != GeomAbs_Plane) {
            result.visibility = WindingFaceVisibility::Unsupported;
            results.push_back(result);
            continue;
        }

        if (faceIndex >= descriptors.value().size())
            return failure<std::vector<WindingFaceVisibilityResult>>(
                ErrorCode::SolverFailure, "Face traversal changed during visibility analysis.", "windingEnvelope.faces");
        const auto role = descriptors.value()[faceIndex].role;
        if (role != WindingEnvelopeFaceRole::OuterLateral &&
            role != WindingEnvelopeFaceRole::AxialEnd) {
            result.visibility = WindingFaceVisibility::Unsupported;
            results.push_back(result);
            continue;
        }

        double uMin = 0.0, uMax = 0.0, vMin = 0.0, vMax = 0.0;
        BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
        gp_Pnt center;
        gp_Vec du, dv;
        surface.D1(0.5*(uMin+uMax), 0.5*(vMin+vMax), center, du, dv);
        gp_Vec normal = du.Crossed(dv);
        if (normal.Magnitude() <= 1.0e-12)
            return failure<std::vector<WindingFaceVisibilityResult>>(
                ErrorCode::SolverFailure, "Planar face has a degenerate normal.", "windingEnvelope.faces");
        normal.Normalize();
        if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();

        for (std::size_t iu = 0; iu < options.samplesPerDirection; ++iu) {
            const double fu = static_cast<double>(iu) /
                              static_cast<double>(options.samplesPerDirection - 1);
            for (std::size_t iv = 0; iv < options.samplesPerDirection; ++iv) {
                const double fv = static_cast<double>(iv) /
                                  static_cast<double>(options.samplesPerDirection - 1);
                const double u = uMin + fu*(uMax-uMin);
                const double v = vMin + fv*(vMax-vMin);
                BRepClass_FaceClassifier classifier(face, gp_Pnt2d(u, v), options.geometricToleranceMm);
                if (classifier.State() != TopAbs_IN && classifier.State() != TopAbs_ON) continue;

                gp_Pnt point;
                gp_Vec sampleDu, sampleDv;
                surface.D1(u, v, point, sampleDu, sampleDv);
                const gp_Pnt rayOrigin = point.Translated(normal * options.rayStartOffsetMm);
                const gp_Lin ray(rayOrigin, gp_Dir(normal));
                bool blocked = false;
                for (auto& intersector : intersectors) {
                    intersector.Perform(ray, 0.0, rayLength);
                    if (intersector.IsDone() && intersector.NbPnt() > 0) {
                        blocked = true;
                        break;
                    }
                }
                ++result.sampleCount;
                if (!blocked) ++result.visibleSampleCount;
            }
        }

        if (result.sampleCount == 0) {
            result.visibility = WindingFaceVisibility::Unsupported;
        } else {
            result.visibleFraction = static_cast<double>(result.visibleSampleCount) /
                                    static_cast<double>(result.sampleCount);
            result.estimatedVisibleAreaMm2 =
                descriptors.value()[faceIndex].areaMm2 * result.visibleFraction;
            if (result.visibleSampleCount == result.sampleCount)
                result.visibility = WindingFaceVisibility::FullyVisible;
            else if (result.visibleSampleCount == 0)
                result.visibility = WindingFaceVisibility::FullyOccluded;
            else
                result.visibility = WindingFaceVisibility::PartiallyOccluded;
        }
        results.push_back(result);
    }
    return Result<std::vector<WindingFaceVisibilityResult>>::success(std::move(results));
}

} // namespace p135::cad
