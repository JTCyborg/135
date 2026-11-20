#include "p135/cad/three_limb_core.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Iterator.hxx>

#include <array>
#include <cassert>
#include <iostream>
#include <cmath>

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
    Standard_Real xMin = 0.0, yMin = 0.0, zMin = 0.0;
    Standard_Real xMax = 0.0, yMax = 0.0, zMax = 0.0;
    bounds.Get(xMin, yMin, zMin, xMax, yMax, zMax);
    assert(std::abs((xMax - xMin) - 240.0) < 1.0e-6);
    assert(std::abs((yMax - yMin) - 200.0) < 1.0e-6);
    assert(std::abs((zMax - zMin) - 80.0) < 1.0e-6);

    std::size_t solidCount = 0;
    for (TopExp_Explorer it(shape.value(), TopAbs_SOLID); it.More(); it.Next()) ++solidCount;
    assert(solidCount == 7);

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
    Standard_Real topAbXMin = 0.0, topAbYMin = 0.0, topAbZMin = 0.0;
    Standard_Real topAbXMax = 0.0, topAbYMax = 0.0, topAbZMax = 0.0;
    Standard_Real topBcXMin = 0.0, topBcYMin = 0.0, topBcZMin = 0.0;
    Standard_Real topBcXMax = 0.0, topBcYMax = 0.0, topBcZMax = 0.0;
    componentBounds[3].Get(topAbXMin, topAbYMin, topAbZMin, topAbXMax, topAbYMax, topAbZMax);
    componentBounds[4].Get(topBcXMin, topBcYMin, topBcZMin, topBcXMax, topBcYMax, topBcZMax);
    assert(std::abs(topAbXMax - 120.0) < 1.0e-6);
    assert(std::abs(topBcXMin - 120.0) < 1.0e-6);

    // Butt interfaces must share the complete limb end face, not overlap
    // volumetrically or leave a gap. The top A interface is 40 x 80 mm.
    TopoDS_Shape limbASolid;
    TopoDS_Shape topAbYokeSolid;
    for (TopExp_Explorer it(shape.value(), TopAbs_SOLID);
         it.More(); it.Next()) {
        const auto& candidate = it.Current();
        Bnd_Box candidateBounds;
        BRepBndLib::Add(candidate, candidateBounds);
        Standard_Real cx0 = 0.0, cy0 = 0.0, cz0 = 0.0;
        Standard_Real cx1 = 0.0, cy1 = 0.0, cz1 = 0.0;
        candidateBounds.Get(cx0, cy0, cz0, cx1, cy1, cz1);
        if (std::abs(cx0 - 0.0) < 1.0e-6 && std::abs(cx1 - 40.0) < 1.0e-6 &&
            std::abs(cy0 - 40.0) < 1.0e-6 && std::abs(cy1 - 160.0) < 1.0e-6)
            limbASolid = candidate;
        if (std::abs(cx0 - 0.0) < 1.0e-6 && std::abs(cx1 - 120.0) < 1.0e-6 &&
            std::abs(cy0 - 160.0) < 1.0e-6 && std::abs(cy1 - 200.0) < 1.0e-6)
            topAbYokeSolid = candidate;
    }
    assert(!limbASolid.IsNull());
    assert(!topAbYokeSolid.IsNull());
    BRepAlgoAPI_Common buttContact(limbASolid, topAbYokeSolid);
    buttContact.Build();
    assert(buttContact.IsDone());
    GProp_GProps contactProperties;
    BRepGProp::SurfaceProperties(buttContact.Shape(), contactProperties);
    assert(std::abs(contactProperties.Mass() - 3200.0) < 1.0e-6);

    auto stepLapGeometry = geometry;
    stepLapGeometry.core.joints[0].construction = CoreJointType::StepLap;
    stepLapGeometry.core.joints[0].stepLapPackets = {
        {units::Length{0.0}, units::Length{0.04}, units::Length{0.02}, units::Length{0.02}},
        {units::Length{0.04}, units::Length{0.04}, units::Length{0.03}, units::Length{0.03}}
    };
    assert(validate(stepLapGeometry.core));
    assert(!p135::cad::buildThreeLimbCoreShape(stepLapGeometry));

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

    auto invalidWindowStack = geometry;
    invalidWindowStack.core.windowWidths[0] = units::Length{0.05};
    assert(!p135::cad::buildThreeLimbCoreShape(invalidWindowStack));
    return 0;
}
