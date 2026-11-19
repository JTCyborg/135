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
    g.core.coreOverallWidth = units::Length{0.24};
    g.core.laminationConstruction = "interleaved step-lap packets";
    for (const auto interface : {
             CoreJointInterface::TopYokeABToLimbA, CoreJointInterface::TopYokeABToLimbB,
             CoreJointInterface::TopYokeBCToLimbB, CoreJointInterface::TopYokeBCToLimbC,
             CoreJointInterface::BottomYokeABToLimbA, CoreJointInterface::BottomYokeABToLimbB,
             CoreJointInterface::BottomYokeBCToLimbB, CoreJointInterface::BottomYokeBCToLimbC}) {
        g.core.joints.push_back({interface, CoreJointType::Butt, {}});
    }
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

    auto missingJoint = valid;
    missingJoint.core.joints.pop_back();
    assert(!validate(missingJoint));

    auto duplicateJoint = valid;
    duplicateJoint.core.joints[1].interface = duplicateJoint.core.joints[0].interface;
    assert(!validate(duplicateJoint));

    auto stepLap = valid;
    stepLap.core.joints[0].construction = CoreJointType::StepLap;
    stepLap.core.joints[0].stepLapPackets = {
        {units::Length{0.0}, units::Length{0.04}, units::Length{0.02}, units::Length{0.02}},
        {units::Length{0.04}, units::Length{0.04}, units::Length{0.03}, units::Length{0.03}}
    };
    assert(validate(stepLap));

    auto missingStepLapPackets = stepLap;
    missingStepLapPackets.core.joints[0].stepLapPackets.clear();
    assert(!validate(missingStepLapPackets));

    auto stepLapDepthGap = stepLap;
    stepLapDepthGap.core.joints[0].stepLapPackets[1].stackDepthOffset = units::Length{0.045};
    assert(!validate(stepLapDepthGap));

    auto stepLapOutOfRange = stepLap;
    stepLapOutOfRange.core.joints[0].stepLapPackets[1].stackDepth = units::Length{0.05};
    assert(!validate(stepLapOutOfRange));

    auto negativeGap = valid;
    negativeGap.core.airGaps.push_back({CoreSegment::LimbA, units::Length{0.02}, units::Length{-0.001}});
    assert(!validate(negativeGap));

    auto gapAtBoundary = valid;
    gapAtBoundary.core.airGaps.push_back({CoreSegment::LimbA, units::Length{0.0}, units::Length{0.001}});
    assert(!validate(gapAtBoundary));

    auto gapOutside = valid;
    gapOutside.core.airGaps.push_back({CoreSegment::LimbA, units::Length{0.12}, units::Length{0.001}});
    assert(!validate(gapOutside));

    auto locatedGap = valid;
    locatedGap.core.airGaps = {
        {CoreSegment::LimbA, units::Length{0.02}, units::Length{0.001}},
        {CoreSegment::LimbA, units::Length{0.04}, units::Length{0.002}},
        {CoreSegment::YokeTopAB, units::Length{0.03}, units::Length{0.001}}
    };
    assert(validate(locatedGap));

    auto overlappingGaps = valid;
    overlappingGaps.core.airGaps = {
        {CoreSegment::LimbB, units::Length{0.02}, units::Length{0.01}},
        {CoreSegment::LimbB, units::Length{0.025}, units::Length{0.01}}
    };
    assert(!validate(overlappingGaps));

    auto invalidSpacing = valid;
    invalidSpacing.core.limbCenterSpacings[2] = units::Length{0.15};
    assert(!validate(invalidSpacing));

    auto inconsistentWindow = valid;
    inconsistentWindow.core.windowWidths[0] = units::Length{0.05};
    assert(!validate(inconsistentWindow));

    auto inconsistentOverallWidth = valid;
    inconsistentOverallWidth.core.coreOverallWidth = units::Length{0.30};
    assert(!validate(inconsistentOverallWidth));

    auto inconsistentWindowHeight = valid;
    inconsistentWindowHeight.core.windowHeights[1] = units::Length{0.10};
    assert(!validate(inconsistentWindowHeight));

    auto nonFinite = valid;
    nonFinite.core.coreOverallWidth = units::Length{std::numeric_limits<double>::infinity()};
    assert(!validate(nonFinite));

    auto missingWindingBuild = valid;
    missingWindingBuild.windingPacks[1].radialBuild = units::Length{};
    assert(!validate(missingWindingBuild));
    return 0;
}
