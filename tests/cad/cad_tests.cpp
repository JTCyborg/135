#include "p135/cad/three_limb_core.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Iterator.hxx>

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>

using namespace p135;
using namespace p135::domain;

namespace {
ReactorPhysicalGeometry validGeometry()
{
    ReactorPhysicalGeometry g;
    g.productId = "CAD-CORE-TEST";
    g.dimensionalSource = "test fixture";
    g.dimensionalRevision = "1";
    g.core.limbWidths = {units::Length{0.04}, units::Length{0.04}, units::Length{0.04}};
    g.core.limbDepths = {units::Length{0.08}, units::Length{0.08}, units::Length{0.08}};
    g.core.windowWidths = {units::Length{0.06}, units::Length{0.06}};
    g.core.windowHeights = {units::Length{0.12}, units::Length{0.12}};
    g.core.yokeThicknesses.fill(units::Length{0.04});
    g.core.yokeDepths.fill(units::Length{0.08});
    g.core.limbCenterSpacings = {units::Length{0.10}, units::Length{0.10}, units::Length{0.20}};
    g.core.coreOverallHeight = units::Length{0.20};
    g.core.coreOverallWidth = units::Length{0.24};
    g.core.laminationConstruction = "test packet construction";
    for (const auto interface : {
             CoreJointInterface::TopYokeABToLimbA, CoreJointInterface::TopYokeABToLimbB,
             CoreJointInterface::TopYokeBCToLimbB, CoreJointInterface::TopYokeBCToLimbC,
             CoreJointInterface::BottomYokeABToLimbA, CoreJointInterface::BottomYokeABToLimbB,
             CoreJointInterface::BottomYokeBCToLimbB, CoreJointInterface::BottomYokeBCToLimbC}) {
        g.core.joints.push_back({interface, CoreJointType::Butt, {}});
    }
    for (auto& b : g.bobbins) {
        b.innerWidth = units::Length{0.035}; b.innerHeight = units::Length{0.11};
        b.axialLength = units::Length{0.075}; b.wallThickness = units::Length{0.001};
        b.flangeThickness = units::Length{0.002}; b.windingClearance = units::Length{0.001};
        b.material = "test polymer";
    }
    for (auto& w : g.windingPacks) {
        w.radialBuild = units::Length{0.012}; w.axialHeight = units::Length{0.10};
        w.endClearance = units::Length{0.005}; w.layerInsulationBuild = units::Length{0.0002};
        w.mainInsulationBuild = units::Length{0.001}; w.conductorInsulationBuild = units::Length{0.0001};
        w.layerCount = 8; w.sections = 2; w.constructionDescription = "test winding";
    }
    return g;
}
}

int main()
{
    const auto geometry = validGeometry();
    const auto shape = p135::cad::buildThreeLimbCoreShape(geometry);
    if (!shape) {
        std::cerr << "CAD build failed: " << shape.error().message << '\\n';
        for (const auto& diagnostic : shape.error().diagnostics) {
            std::cerr << diagnostic.code << ": " << diagnostic.message;
            if (diagnostic.fieldPath) {
                std::cerr << " [field=" << *diagnostic.fieldPath << "]";
            }
            std::cerr << '\\n';
        }
    }
    assert(shape);
    assert(!shape.value().IsNull());
    assert(BRepCheck_Analyzer(shape.value()).IsValid());

    Bnd_Box bounds;
    BRepBndLib::Add(shape.value(), bounds);
    double xMin = 0.0, yMin = 0.0, zMin = 0.0;
    double xMax = 0.0, yMax = 0.0, zMax = 0.0;
    bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    assert(std::abs((xMax - xMin) - 240.0) < 1.0e-6);
    assert(std::abs((yMax - yMin) - 200.0) < 1.0e-6);
    assert(std::abs((zMax - zMin) - 80.0) < 1.0e-6);

    std::size_t solidCount = 0;
    for (TopExp_Explorer it(shape.value(), TopAbs_SOLID); it.More(); it.Next()) {
        ++solidCount;
        GProp_GProps volumeProperties;
        BRepGProp::VolumeProperties(it.Current(), volumeProperties);
        assert(std::isfinite(volumeProperties.Mass()));
        assert(volumeProperties.Mass() > 1.0e-12);
    }
    assert(solidCount == 7);

    // The assembly is intentionally a compound, not a fused solid. Its
    // members may share contact faces, but no pair may overlap in volume.
    std::vector<TopoDS_Shape> baseSolids;
    for (TopExp_Explorer it(shape.value(), TopAbs_SOLID); it.More(); it.Next())
        baseSolids.push_back(it.Current());
    for (std::size_t i = 0; i < baseSolids.size(); ++i) {
        for (std::size_t j = i + 1; j < baseSolids.size(); ++j) {
            BRepAlgoAPI_Common common(baseSolids[i], baseSolids[j]);
            common.Build();
            assert(common.IsDone());
            GProp_GProps commonVolume;
            BRepGProp::VolumeProperties(common.Shape(), commonVolume);
            assert(std::abs(commonVolume.Mass()) < 1.0e-8);
        }
    }

    const auto volumeOf = [](const TopoDS_Shape& candidate) {
        GProp_GProps properties;
        BRepGProp::VolumeProperties(candidate, properties);
        return properties.Mass();
    };
    // Base assembly volume: 3 limbs (40 x 120 x 80 mm) and
    // 4 yoke halves (120 x 40 x 80 mm), with face-only contacts.
    constexpr double baseVolumeMm3 =
        3.0 * 40.0 * 120.0 * 80.0 + 4.0 * 120.0 * 40.0 * 80.0;
    assert(std::abs(volumeOf(shape.value()) - baseVolumeMm3) < 1.0e-6);

    // Top-level component order is limbs A/B/C, then yokes top AB/BC,
    // bottom AB/BC. The two top yoke halves must meet at x = 120 mm
    // without overlapping around the middle limb.
    std::array<Bnd_Box, 7> componentBounds;
    std::size_t componentIndex = 0;
    for (TopoDS_Iterator it(shape.value()); it.More(); it.Next()) {
        assert(componentIndex < componentBounds.size());
        BRepBndLib::Add(it.Value(), componentBounds[componentIndex]);
        ++componentIndex;
    }
    assert(componentIndex == componentBounds.size());
    double topAbXMin = 0.0, topAbYMin = 0.0, topAbZMin = 0.0;
    double topAbXMax = 0.0, topAbYMax = 0.0, topAbZMax = 0.0;
    double topBcXMin = 0.0, topBcYMin = 0.0, topBcZMin = 0.0;
    double topBcXMax = 0.0, topBcYMax = 0.0, topBcZMax = 0.0;
    componentBounds[3].Get(topAbXMin, topAbYMin, topAbZMin, topAbXMax, topAbYMax, topAbZMax);
    componentBounds[4].Get(topBcXMin, topBcYMin, topBcZMin, topBcXMax, topBcYMax, topBcZMax);
    assert(std::abs(topAbXMax - 120.0) < 1.0e-6);
    assert(std::abs(topBcXMin - 120.0) < 1.0e-6);

    // Verify all eight Butt-joint interfaces geometrically. Each limb end
    // face must mate with the corresponding yoke face. Outer limbs contact
    // over their full width; the middle limb is split at its centre plane.
    const auto findPlanarFace = [](const TopoDS_Shape& solid, double planeY,
                                   double x0, double x1, double z0, double z1) {
        TopoDS_Face result;
        for (TopExp_Explorer it(solid, TopAbs_FACE); it.More(); it.Next()) {
            const auto face = TopoDS::Face(it.Current());
            if (BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane) continue;
            Bnd_Box faceBounds;
            BRepBndLib::Add(face, faceBounds);
            double fx0 = 0.0, fy0 = 0.0, fz0 = 0.0;
            double fx1 = 0.0, fy1 = 0.0, fz1 = 0.0;
            faceBounds.Get(fx0, fy0, fz0, fx1, fy1, fz1);
            constexpr double tolerance = 1.0e-6;
            if (std::abs(fy0 - planeY) < tolerance &&
                std::abs(fy1 - planeY) < tolerance &&
                std::abs(fx0 - x0) < tolerance &&
                std::abs(fx1 - x1) < tolerance &&
                std::abs(fz0 - z0) < tolerance &&
                std::abs(fz1 - z1) < tolerance) {
                result = face;
                break;
            }
        }
        return result;
    };

    std::array<TopoDS_Shape, 3> limbSolids;
    std::array<TopoDS_Shape, 4> yokeSolids;
    for (TopExp_Explorer it(shape.value(), TopAbs_SOLID); it.More(); it.Next()) {
        const auto& candidate = it.Current();
        Bnd_Box candidateBounds;
        BRepBndLib::Add(candidate, candidateBounds);
        double x0 = 0.0, y0 = 0.0, z0 = 0.0;
        double x1 = 0.0, y1 = 0.0, z1 = 0.0;
        candidateBounds.Get(x0, y0, z0, x1, y1, z1);
        for (std::size_t limb = 0; limb < 3; ++limb) {
            const std::array<double, 3> limbX0{0.0, 100.0, 200.0};
            const std::array<double, 3> limbX1{40.0, 140.0, 240.0};
            if (std::abs(x0 - limbX0[limb]) < 1.0e-6 &&
                std::abs(x1 - limbX1[limb]) < 1.0e-6 &&
                std::abs(y0 - 40.0) < 1.0e-6 &&
                std::abs(y1 - 160.0) < 1.0e-6)
                limbSolids[limb] = candidate;
        }
        for (std::size_t yoke = 0; yoke < 4; ++yoke) {
            const bool isAb = (yoke % 2) == 0;
            const bool isTop = yoke < 2;
            const double expectedX0 = isAb ? 0.0 : 120.0;
            const double expectedX1 = isAb ? 120.0 : 240.0;
            const double expectedY0 = isTop ? 160.0 : 0.0;
            const double expectedY1 = isTop ? 200.0 : 40.0;
            if (std::abs(x0 - expectedX0) < 1.0e-6 &&
                std::abs(x1 - expectedX1) < 1.0e-6 &&
                std::abs(y0 - expectedY0) < 1.0e-6 &&
                std::abs(y1 - expectedY1) < 1.0e-6)
                yokeSolids[yoke] = candidate;
        }
    }
    for (const auto& limb : limbSolids) assert(!limb.IsNull());
    for (const auto& yoke : yokeSolids) assert(!yoke.IsNull());

    struct JointExpectation {
        std::size_t limb;
        std::size_t yoke;
        double planeY;
        double limbX0;
        double limbX1;
    };
    const std::array<JointExpectation, 8> joints{{
        {0, 0, 160.0, 0.0, 40.0},   // top AB -> limb A
        {1, 0, 160.0, 100.0, 140.0},// top AB -> limb B
        {1, 1, 160.0, 100.0, 140.0},// top BC -> limb B
        {2, 1, 160.0, 200.0, 240.0},// top BC -> limb C
        {0, 2, 40.0, 0.0, 40.0},    // bottom AB -> limb A
        {1, 2, 40.0, 100.0, 140.0}, // bottom AB -> limb B
        {1, 3, 40.0, 100.0, 140.0}, // bottom BC -> limb B
        {2, 3, 40.0, 200.0, 240.0}  // bottom BC -> limb C
    }};
    for (const auto& joint : joints) {
        const auto limbFace = findPlanarFace(
            limbSolids[joint.limb], joint.planeY,
            joint.limbX0, joint.limbX1, 0.0, 80.0);
        const double yokeX0 = (joint.yoke % 2) == 0 ? 0.0 : 120.0;
        const double yokeX1 = (joint.yoke % 2) == 0 ? 120.0 : 240.0;
        const auto yokeFace = findPlanarFace(
            yokeSolids[joint.yoke], joint.planeY,
            yokeX0, yokeX1, 0.0, 80.0);
        assert(!limbFace.IsNull());
        assert(!yokeFace.IsNull());

        GProp_GProps limbFaceProperties;
        GProp_GProps yokeFaceProperties;
        BRepGProp::SurfaceProperties(limbFace, limbFaceProperties);
        BRepGProp::SurfaceProperties(yokeFace, yokeFaceProperties);
        assert(std::abs(limbFaceProperties.Mass() - 3200.0) < 1.0e-6);
        const double expectedYokeArea = (yokeX1 - yokeX0) * 80.0;
        assert(std::abs(yokeFaceProperties.Mass() - expectedYokeArea) < 1.0e-6);

        Bnd_Box limbFaceBounds;
        Bnd_Box yokeFaceBounds;
        BRepBndLib::Add(limbFace, limbFaceBounds);
        BRepBndLib::Add(yokeFace, yokeFaceBounds);
        double lx0 = 0.0, ly0 = 0.0, lz0 = 0.0;
        double lx1 = 0.0, ly1 = 0.0, lz1 = 0.0;
        double yx0 = 0.0, yy0 = 0.0, yz0 = 0.0;
        double yx1 = 0.0, yy1 = 0.0, yz1 = 0.0;
        limbFaceBounds.Get(lx0, ly0, lz0, lx1, ly1, lz1);
        yokeFaceBounds.Get(yx0, yy0, yz0, yx1, yy1, yz1);
        assert(std::abs(ly0 - yy0) < 1.0e-6);
        assert(std::abs(ly1 - yy1) < 1.0e-6);
        const double overlapX = std::min(lx1, yx1) - std::max(lx0, yx0);
        const double overlapZ = std::min(lz1, yz1) - std::max(lz0, yz0);
        const double expectedOverlapX = joint.limb == 1 ? 20.0 : 40.0;
        assert(std::abs(overlapX - expectedOverlapX) < 1.0e-6);
        assert(std::abs(overlapZ - 80.0) < 1.0e-6);
    }

    auto stepLapGeometry = geometry;
    stepLapGeometry.core.joints[0].construction = CoreJointType::StepLap;
    stepLapGeometry.core.joints[0].stepLapPackets = {
        {units::Length{0.0}, units::Length{0.04}, units::Length{0.02}, units::Length{0.02}, StepLapPacketDirection::LimbIntoYoke},
        {units::Length{0.04}, units::Length{0.04}, units::Length{0.03}, units::Length{0.03}, StepLapPacketDirection::YokeIntoLimb}
    };
    assert(validate(stepLapGeometry.core));
    const auto unsupportedStepLap = p135::cad::buildThreeLimbCoreShape(stepLapGeometry);
    assert(!unsupportedStepLap);
    assert(unsupportedStepLap.error().code == ErrorCode::UnsupportedConfiguration);
    assert(!unsupportedStepLap.error().diagnostics.empty());
    assert(unsupportedStepLap.error().diagnostics.front().code == "CAD_STEP_LAP_NOT_IMPLEMENTED");

    auto invalid = geometry;
    invalid.core.coreOverallHeight = units::Length{0.05};
    assert(!p135::cad::buildThreeLimbCoreShape(invalid));

    auto geometryWithGaps = geometry;
    geometryWithGaps.core.airGaps = {
        {CoreSegment::LimbA, units::Length{0.02}, units::Length{0.001}},
        {CoreSegment::LimbA, units::Length{0.04}, units::Length{0.002}},
        {CoreSegment::YokeTopAB, units::Length{0.03}, units::Length{0.001}}
    };
    const auto gappedShape = p135::cad::buildThreeLimbCoreShape(geometryWithGaps);
    assert(gappedShape);
    std::size_t gappedSolidCount = 0;
    for (TopExp_Explorer it(gappedShape.value(), TopAbs_SOLID); it.More(); it.Next()) ++gappedSolidCount;
    assert(gappedSolidCount == 10);
    assert(BRepCheck_Analyzer(gappedShape.value()).IsValid());
    // Two limb-A gaps remove 40 x 80 x (1 + 2) mm3.
    // A yoke gap is cut along the yoke length (X), so its removed
    // cross-section is yoke thickness (Y) x stack depth (Z): 40 x 80 x 1.
    const double expectedGappedVolume = baseVolumeMm3 -
        (40.0 * 80.0 * 3.0 + 40.0 * 80.0 * 1.0);
    assert(std::abs(volumeOf(gappedShape.value()) - expectedGappedVolume) < 1.0e-6);

    // Adjacent air-gap intervals are a single continuous cut. They must not
    // make the CAD segmenter attempt to create a zero-length intermediate solid.
    auto adjacentGaps = geometry;
    adjacentGaps.core.airGaps = {
        {CoreSegment::LimbA, units::Length{0.02}, units::Length{0.001}},
        {CoreSegment::LimbA, units::Length{0.021}, units::Length{0.002}}
    };
    const auto adjacentGapShape = p135::cad::buildThreeLimbCoreShape(adjacentGaps);
    assert(adjacentGapShape);
    std::size_t adjacentGapSolidCount = 0;
    for (TopExp_Explorer it(adjacentGapShape.value(), TopAbs_SOLID); it.More(); it.Next())
        ++adjacentGapSolidCount;
    assert(adjacentGapSolidCount == 8);
    assert(BRepCheck_Analyzer(adjacentGapShape.value()).IsValid());
    // Touching gaps merge into one 3 mm cut, not a zero-thickness sliver.
    const double expectedAdjacentGapVolume = baseVolumeMm3 - 40.0 * 80.0 * 3.0;
    assert(std::abs(volumeOf(adjacentGapShape.value()) - expectedAdjacentGapVolume) < 1.0e-6);

    // The top AB yoke half spans 120 mm from the outside face of limb A
    // to the centre plane of limb B. This gap is near its right-hand end.
    auto yokeGapNearEnd = geometry;
    yokeGapNearEnd.core.airGaps = {
        {CoreSegment::YokeTopAB, units::Length{0.10}, units::Length{0.01}}
    };
    const auto yokeGapShape = p135::cad::buildThreeLimbCoreShape(yokeGapNearEnd);
    assert(yokeGapShape);
    std::size_t yokeGapSolidCount = 0;
    for (TopExp_Explorer it(yokeGapShape.value(), TopAbs_SOLID); it.More(); it.Next()) ++yokeGapSolidCount;
    assert(yokeGapSolidCount == 8);
    assert(BRepCheck_Analyzer(yokeGapShape.value()).IsValid());
    // A 10 mm cut along the yoke length removes 10 x 40 x 80 mm3.
    assert(std::abs(volumeOf(yokeGapShape.value()) -
                    (baseVolumeMm3 - 10.0 * 40.0 * 80.0)) < 1.0e-6);

    auto invalidWindowStack = geometry;
    invalidWindowStack.core.windowWidths[0] = units::Length{0.05};
    assert(!p135::cad::buildThreeLimbCoreShape(invalidWindowStack));
    return 0;
}
