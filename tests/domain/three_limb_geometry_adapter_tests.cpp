#include "p135/domain/three_limb_geometry_adapter.hpp"

#include <array>
#include <cassert>
#include <cmath>

using namespace p135;
using namespace p135::domain;

namespace {

ThreeLimbCorePhysicalGeometry validCore()
{
    ThreeLimbCorePhysicalGeometry g;
    g.limbWidths = {units::Length{0.04}, units::Length{0.04}, units::Length{0.04}};
    g.limbDepths = {units::Length{0.08}, units::Length{0.08}, units::Length{0.08}};
    g.windowWidths = {units::Length{0.06}, units::Length{0.06}};
    g.windowHeights = {units::Length{0.12}, units::Length{0.12}};
    g.yokeThicknesses = {
        units::Length{0.03}, units::Length{0.03},
        units::Length{0.04}, units::Length{0.04}
    };
    g.yokeDepths.fill(units::Length{0.08});
    g.limbCenterSpacings = {
        units::Length{0.10}, units::Length{0.10}, units::Length{0.20}
    };
    g.coreOverallHeight = units::Length{0.20};
    g.coreOverallWidth = units::Length{0.24};
    g.laminationConstruction = "interleaved step-lap packets";
    for (const auto interface : {
             CoreJointInterface::TopYokeABToLimbA, CoreJointInterface::TopYokeABToLimbB,
             CoreJointInterface::TopYokeBCToLimbB, CoreJointInterface::TopYokeBCToLimbC,
             CoreJointInterface::BottomYokeABToLimbA, CoreJointInterface::BottomYokeABToLimbB,
             CoreJointInterface::BottomYokeBCToLimbB, CoreJointInterface::BottomYokeBCToLimbC}) {
        g.joints.push_back({interface, CoreJointType::Butt, {}});
    }
    return g;
}

bool near(double lhs, double rhs, double tolerance = 1.0e-12)
{
    return std::abs(lhs - rhs) <= tolerance;
}

} // namespace

int main()
{
    const auto physical = validCore();
    const auto converted = buildThreeLimbMagneticGeometry(physical, 0.95);
    assert(converted);

    const auto& magnetic = converted.value();
    const std::array<double, 3> expectedPaths{0.165, 0.165, 0.165};
    for (std::size_t limb = 0; limb < 3; ++limb) {
        assert(near(magnetic.limbEffectiveAreas[limb]->si(), 0.04 * 0.08 * 0.95));
        assert(near(magnetic.limbMeanPaths[limb]->si(), expectedPaths[limb]));
        assert(near(magnetic.limbAirGaps[limb]->si(), 0.0));
        assert(near(magnetic.limbGapEffectiveAreas[limb]->si(), 0.04 * 0.08 * 0.95));
    }
    const std::array<double, 4> yokeThicknesses{0.03, 0.03, 0.04, 0.04};
    for (std::size_t yoke = 0; yoke < 4; ++yoke) {
        assert(near(magnetic.yokeEffectiveAreas[yoke]->si(),
                    yokeThicknesses[yoke] * 0.08 * 0.95));
        assert(near(magnetic.yokeSegmentPaths[yoke]->si(), 0.10));
    }

    auto withLimbGaps = physical;
    withLimbGaps.airGaps = {
        {CoreSegment::LimbA, units::Length{0.02}, units::Length{0.001}},
        {CoreSegment::LimbA, units::Length{0.04}, units::Length{0.002}},
        {CoreSegment::LimbC, units::Length{0.03}, units::Length{0.0015}}
    };
    const auto gapped = buildThreeLimbMagneticGeometry(withLimbGaps, 1.0);
    assert(gapped);
    assert(near(gapped.value().limbAirGaps[0]->si(), 0.003));
    assert(near(gapped.value().limbAirGaps[1]->si(), 0.0));
    assert(near(gapped.value().limbAirGaps[2]->si(), 0.0015));

    assert(!buildThreeLimbMagneticGeometry(physical, 0.0));
    assert(!buildThreeLimbMagneticGeometry(physical, 1.01));
    assert(!buildThreeLimbMagneticGeometry(physical, NAN));

    auto withYokeGap = physical;
    withYokeGap.airGaps = {
        {CoreSegment::YokeTopAB, units::Length{0.03}, units::Length{0.001}}
    };
    const auto unsupported = buildThreeLimbMagneticGeometry(withYokeGap, 0.95);
    assert(!unsupported);
    assert(unsupported.error().code == ErrorCode::InvalidInput);

    auto invalidPhysical = physical;
    invalidPhysical.limbWidths[0] = units::Length{};
    assert(!buildThreeLimbMagneticGeometry(invalidPhysical, 0.95));
    return 0;
}
