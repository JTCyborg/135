#include "p135/domain/mechanical_geometry.hpp"

#include <cassert>
#include <limits>
#include <utility>

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
        b.innerWidth = units::Length{0.045};
        b.innerHeight = units::Length{0.11};
        b.axialLength = units::Length{0.115};
        b.wallThickness = units::Length{0.001};
        b.flangeThickness = units::Length{0.002};
        b.flangeOuterWidth = units::Length{0.055};
        b.flangeOuterHeight = units::Length{0.12};
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
        {units::Length{0.0}, units::Length{0.04}, units::Length{0.02}, units::Length{0.02}, StepLapPacketDirection::LimbIntoYoke},
        {units::Length{0.04}, units::Length{0.04}, units::Length{0.03}, units::Length{0.03}, StepLapPacketDirection::YokeIntoLimb}
    };
    assert(validate(stepLap));

    // The same packet-depth contract must validate at each of the eight
    // physical limb/yoke interfaces, not only the first AB/top interface.
    auto stepLapAtEveryInterface = valid;
    for (auto& joint : stepLapAtEveryInterface.core.joints) {
        joint.construction = CoreJointType::StepLap;
        joint.stepLapPackets = stepLap.core.joints[0].stepLapPackets;
    }
    assert(validate(stepLapAtEveryInterface));

    auto excessiveYokeOverlap = stepLap;
    excessiveYokeOverlap.core.joints[0].stepLapPackets[0].overlapIntoYoke = units::Length{0.13};
    assert(!validate(excessiveYokeOverlap));

    auto unorderedStepLap = stepLap;
    std::swap(unorderedStepLap.core.joints[0].stepLapPackets[0],
              unorderedStepLap.core.joints[0].stepLapPackets[1]);
    assert(!validate(unorderedStepLap));

    auto mismatchedStepLapDepth = stepLap;
    mismatchedStepLapDepth.core.yokeDepths[0] = units::Length{0.07};
    assert(!validate(mismatchedStepLapDepth));

    auto excessiveStepLapOverlap = stepLap;
    excessiveStepLapOverlap.core.joints[0].stepLapPackets[0].overlapIntoLimb = units::Length{0.13};
    assert(!validate(excessiveStepLapOverlap));

    auto excessiveYokeThicknessOverlap = stepLap;
    excessiveYokeThicknessOverlap.core.joints[0].stepLapPackets[0].overlapIntoYoke = units::Length{0.05};
    assert(!validate(excessiveYokeThicknessOverlap));

    auto nonAlternatingStepLap = stepLap;
    nonAlternatingStepLap.core.joints[0].stepLapPackets[1].direction = StepLapPacketDirection::LimbIntoYoke;
    assert(!validate(nonAlternatingStepLap));

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

    // The AB yoke half runs from the outside face of A to the centre
    // plane of B: 40 mm + 60 mm + 20 mm = 120 mm.
    auto yokeGapNearPhysicalEnd = valid;
    yokeGapNearPhysicalEnd.core.airGaps = {
        {CoreSegment::YokeTopAB, units::Length{0.10}, units::Length{0.01}}
    };
    assert(validate(yokeGapNearPhysicalEnd));

    auto yokeGapBeyondPhysicalEnd = valid;
    yokeGapBeyondPhysicalEnd.core.airGaps = {
        {CoreSegment::YokeTopAB, units::Length{0.119}, units::Length{0.002}}
    };
    assert(!validate(yokeGapBeyondPhysicalEnd));

    auto overlappingGaps = valid;
    overlappingGaps.core.airGaps = {
        {CoreSegment::LimbB, units::Length{0.02}, units::Length{0.01}},
        {CoreSegment::LimbB, units::Length{0.025}, units::Length{0.01}}
    };
    assert(!validate(overlappingGaps));

    auto mismatchedButtDepth = valid;
    mismatchedButtDepth.core.yokeDepths[0] = units::Length{0.07};
    assert(!validate(mismatchedButtDepth));

    auto mismatchedTopYokeThickness = valid;
    mismatchedTopYokeThickness.core.yokeThicknesses[1] = units::Length{0.045};
    assert(!validate(mismatchedTopYokeThickness));

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

    // Conductor-level data is optional for legacy envelope-only geometry,
    // but once any detail is supplied the complete, internally consistent
    // round-wire definition is required.
    auto detailedWinding = valid;
    for (auto& winding : detailedWinding.windingPacks) {
        winding.turns = 20;
        winding.turnsPerLayer = 10;
        winding.parallelConductors = 1;
        winding.conductorShape = "round";
        winding.bareConductorDiameter = units::Length{0.002};
        winding.insulatedConductorDiameter = units::Length{0.0022};
        winding.conductorInsulationBuild = units::Length{0.0001};
    }
    assert(validate(detailedWinding));

    auto incompleteConductorDetail = detailedWinding;
    incompleteConductorDetail.windingPacks[0].insulatedConductorDiameter.reset();
    assert(!validate(incompleteConductorDetail));

    auto inconsistentTurnCount = detailedWinding;
    inconsistentTurnCount.windingPacks[0].turns = 21;
    assert(!validate(inconsistentTurnCount));

    auto rectangularWinding = valid;
    for (auto& winding : rectangularWinding.windingPacks) {
        winding.turns = 120;
        winding.turnsPerLayer = 12;
        winding.parallelConductors = 2;
        winding.conductorShape = "rectangular";
        winding.bareConductorWidth = units::Length{0.002};
        winding.bareConductorThickness = units::Length{0.00125};
    }
    // 120 turns cannot fit 8 layers x 12 turns/layer: the geometry is rejected.
    assert(!validate(rectangularWinding));
    for (auto& winding : rectangularWinding.windingPacks) {
        winding.layerCount = 10;
    }
    assert(validate(rectangularWinding));

    auto unsupportedConductorShape = detailedWinding;
    unsupportedConductorShape.windingPacks[0].conductorShape = "litz";
    assert(!validate(unsupportedConductorShape));

    auto impossibleInsulatedDiameter = detailedWinding;
    impossibleInsulatedDiameter.windingPacks[0].insulatedConductorDiameter = units::Length{0.0019};
    assert(!validate(impossibleInsulatedDiameter));

    auto inconsistentInsulationBuild = detailedWinding;
    inconsistentInsulationBuild.windingPacks[0].conductorInsulationBuild = units::Length{0.0002};
    assert(!validate(inconsistentInsulationBuild));

    auto bobbinTooNarrowForLimb = valid;
    bobbinTooNarrowForLimb.bobbins[0].innerWidth = units::Length{0.039};
    assert(!validate(bobbinTooNarrowForLimb));

    auto bobbinTooShallowForStack = valid;
    bobbinTooShallowForStack.bobbins[2].innerHeight = units::Length{0.079};
    assert(!validate(bobbinTooShallowForStack));

    auto bobbinTooLongForWindow = valid;
    bobbinTooLongForWindow.bobbins[1].axialLength = units::Length{0.121};
    assert(!validate(bobbinTooLongForWindow));

    auto middleBobbinMustFitBothWindows = valid;
    middleBobbinMustFitBothWindows.core.windowHeights = {
        units::Length{0.10}, units::Length{0.10}
    };
    middleBobbinMustFitBothWindows.core.coreOverallHeight = units::Length{0.18};
    middleBobbinMustFitBothWindows.bobbins[1].axialLength = units::Length{0.105};
    assert(!validate(middleBobbinMustFitBothWindows));

    auto windingExceedsBobbinAxialLength = valid;
    windingExceedsBobbinAxialLength.windingPacks[0].axialHeight = units::Length{0.106};
    assert(!validate(windingExceedsBobbinAxialLength));

    auto windingFitsBobbinAxialLength = valid;
    windingFitsBobbinAxialLength.windingPacks[0].axialHeight = units::Length{0.105};
    assert(validate(windingFitsBobbinAxialLength));

    auto missingBobbinFlangeEnvelope = valid;
    missingBobbinFlangeEnvelope.bobbins[0].flangeOuterWidth = units::Length{};
    assert(!validate(missingBobbinFlangeEnvelope));

    auto flangeTooThickForBobbin = valid;
    flangeTooThickForBobbin.bobbins[0].flangeThickness = units::Length{0.06};
    assert(!validate(flangeTooThickForBobbin));
    return 0;
}
