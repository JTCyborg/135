#include "p135/domain/mechanical_geometry.hpp"

#include <cassert>
#include <limits>

using namespace p135;
using namespace p135::domain;

namespace {
ReactorPhysicalGeometry validGeometry()
{
    ReactorPhysicalGeometry g;
    g.productId = "REACTOR-3PH-001";
    g.dimensionalSource = "controlled manufacturing drawing";
    g.dimensionalRevision = "A";
    g.core.limbWidths = {units::Length{0.04}, units::Length{0.04}, units::Length{0.04}};
    g.core.limbDepths = {units::Length{0.08}, units::Length{0.08}, units::Length{0.08}};
    g.core.windowWidths = {units::Length{0.06}, units::Length{0.06}};
    g.core.windowHeights = {units::Length{0.12}, units::Length{0.12}};
    g.core.yokeThicknesses.fill(units::Length{0.04});
    g.core.yokeDepths.fill(units::Length{0.08});
    g.core.limbCenterSpacings = {units::Length{0.10}, units::Length{0.10}, units::Length{0.20}};
    g.core.coreOverallHeight = units::Length{0.20};
    g.core.coreOverallWidth = units::Length{0.30};
    g.core.laminationConstruction = "interleaved step-lap packets";
    for (auto& b : g.bobbins) {
        b.innerWidth = units::Length{0.035};
        b.innerHeight = units::Length{0.11};
        b.axialLength = units::Length{0.075};
        b.wallThickness = units::Length{0.001};
        b.flangeThickness = units::Length{0.002};
        b.windingClearance = units::Length{0.001};
        b.material = "PBT";
    }
    for (auto& w : g.windingPacks) {
        w.radialBuild = units::Length{0.012};
        w.axialHeight = units::Length{0.10};
        w.endClearance = units::Length{0.005};
        w.layerInsulationBuild = units::Length{0.0002};
        w.mainInsulationBuild = units::Length{0.001};
        w.conductorInsulationBuild = units::Length{0.0001};
        w.layerCount = 8;
        w.sections = 2;
        w.constructionDescription = "8 layers, 2 sections";
    }
    return g;
}
}

int main()
{
    const auto valid = validGeometry();
    assert(validate(valid));
    assert(!validate(ThreeLimbCorePhysicalGeometry{}));
    assert(!validate(BobbinPhysicalGeometry{}));
    assert(!validate(WindingPackPhysicalGeometry{}));

    auto missingSource = valid;
    missingSource.dimensionalSource.clear();
    assert(!validate(missingSource));

    auto negativeGap = valid;
    negativeGap.core.airGapLengths[0] = units::Length{-0.001};
    assert(!validate(negativeGap));

    auto unlocatedGap = valid;
    unlocatedGap.core.airGapLengths[0] = units::Length{0.001};
    assert(!validate(unlocatedGap));

    auto locatedGap = unlocatedGap;
    locatedGap.core.airGapLocation = AirGapLocation::LimbA;
    assert(validate(locatedGap));

    auto invalidSpacing = valid;
    invalidSpacing.core.limbCenterSpacings[2] = units::Length{0.15};
    assert(!validate(invalidSpacing));

    auto nonFinite = valid;
    nonFinite.core.coreOverallWidth = units::Length{std::numeric_limits<double>::infinity()};
    assert(!validate(nonFinite));

    auto missingWindingBuild = valid;
    missingWindingBuild.windingPacks[1].radialBuild = units::Length{};
    assert(!validate(missingWindingBuild));
    return 0;
}
