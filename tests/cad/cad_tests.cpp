#include "p135/cad/three_limb_core.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <cassert>

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
    g.core.coreOverallWidth = units::Length{0.30};
    g.core.laminationConstruction = "test packet construction";
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
    assert(shape);
    assert(!shape.value().IsNull());
    assert(BRepCheck_Analyzer(shape.value()).IsValid());
    std::size_t solidCount = 0;
    for (TopExp_Explorer it(shape.value(), TopAbs_SOLID); it.More(); it.Next()) ++solidCount;
    assert(solidCount == 7);

    auto invalid = geometry;
    invalid.core.coreOverallHeight = units::Length{0.05};
    assert(!p135::cad::buildThreeLimbCoreShape(invalid));
    return 0;
}
