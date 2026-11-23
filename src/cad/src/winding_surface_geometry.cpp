#include "p135/cad/winding_surface_geometry.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

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
        Severity::Error, "CAD_WINDING_FACE_EXTRACTION_FAILED", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<T>::failure(std::move(error));
}

} // namespace

Result<std::vector<WindingEnvelopeFaceGeometry>>
extractWindingEnvelopeFaceGeometry(const TopoDS_Shape& windingEnvelope)
{
    if (windingEnvelope.IsNull())
        return failure<std::vector<WindingEnvelopeFaceGeometry>>(
            ErrorCode::InvalidInput, "Winding envelope shape is null.", "windingEnvelope");
    if (!BRepCheck_Analyzer(windingEnvelope).IsValid())
        return failure<std::vector<WindingEnvelopeFaceGeometry>>(
            ErrorCode::InvalidInput, "Winding envelope shape is not a valid B-Rep.", "windingEnvelope");

    std::vector<WindingEnvelopeFaceGeometry> faces;
    std::size_t faceIndex = 0;
    for (TopExp_Explorer it(windingEnvelope, TopAbs_FACE); it.More(); it.Next(), ++faceIndex) {
        const TopoDS_Face face = TopoDS::Face(it.Current());
        GProp_GProps surfaceProperties;
        BRepGProp::SurfaceProperties(face, surfaceProperties);
        const double area = surfaceProperties.Mass();
        const gp_Pnt centroid = surfaceProperties.CentreOfMass();
        if (!std::isfinite(area) || area <= 0.0 ||
            !std::isfinite(centroid.X()) || !std::isfinite(centroid.Y()) ||
            !std::isfinite(centroid.Z()))
            return failure<std::vector<WindingEnvelopeFaceGeometry>>(
                ErrorCode::SolverFailure,
                "OCCT returned invalid area or centroid for a winding-envelope face.",
                "windingEnvelope.faces[" + std::to_string(faceIndex) + "]");

        WindingEnvelopeFaceGeometry descriptor;
        descriptor.faceIndex = faceIndex;
        descriptor.areaMm2 = area;
        descriptor.centroidMm = {centroid.X(), centroid.Y(), centroid.Z()};

        Bnd_Box bounds;
        BRepBndLib::Add(face, bounds);
        if (bounds.IsVoid() || bounds.IsOpenXmin() || bounds.IsOpenXmax() ||
            bounds.IsOpenYmin() || bounds.IsOpenYmax() ||
            bounds.IsOpenZmin() || bounds.IsOpenZmax())
            return failure<std::vector<WindingEnvelopeFaceGeometry>>(
                ErrorCode::SolverFailure,
                "OCCT returned unbounded face geometry for a winding envelope.",
                "windingEnvelope.faces[" + std::to_string(faceIndex) + "]");
        bounds.Get(descriptor.boundsMm[0], descriptor.boundsMm[1], descriptor.boundsMm[2],
                   descriptor.boundsMm[3], descriptor.boundsMm[4], descriptor.boundsMm[5]);

        BRepAdaptor_Surface surface(face, true);
        descriptor.planar = surface.GetType() == GeomAbs_Plane;
        if (descriptor.planar) {
            double uMin = 0.0, uMax = 0.0, vMin = 0.0, vMax = 0.0;
            BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
            gp_Pnt point;
            gp_Vec du, dv;
            surface.D1(0.5 * (uMin + uMax), 0.5 * (vMin + vMax), point, du, dv);
            gp_Vec normal = du.Crossed(dv);
            const double magnitude = normal.Magnitude();
            if (!std::isfinite(magnitude) || magnitude <= 1.0e-12)
                return failure<std::vector<WindingEnvelopeFaceGeometry>>(
                    ErrorCode::SolverFailure,
                    "OCCT returned a degenerate planar-face normal.",
                    "windingEnvelope.faces[" + std::to_string(faceIndex) + "]");
            normal /= magnitude;
            if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();
            descriptor.unitNormal = {normal.X(), normal.Y(), normal.Z()};
        }
        faces.push_back(descriptor);
    }

    if (faces.empty())
        return failure<std::vector<WindingEnvelopeFaceGeometry>>(
            ErrorCode::InvalidInput, "Winding envelope contains no faces.", "windingEnvelope");
    return Result<std::vector<WindingEnvelopeFaceGeometry>>::success(std::move(faces));
}

} // namespace p135::cad
