#include "p135/domain/three_limb_core_mass.hpp"

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
    g.windowHeights = {units::Length{0.13}, units::Length{0.13}};
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
    g.laminationConstruction = "stacked electrical-steel laminations; butt joints";
    for (const auto interface : {
             CoreJointInterface::TopYokeABToLimbA, CoreJointInterface::TopYokeABToLimbB,
             CoreJointInterface::TopYokeBCToLimbB, CoreJointInterface::TopYokeBCToLimbC,
             CoreJointInterface::BottomYokeABToLimbA, CoreJointInterface::BottomYokeABToLimbB,
             CoreJointInterface::BottomYokeBCToLimbB, CoreJointInterface::BottomYokeBCToLimbC}) {
        g.joints.push_back({interface, CoreJointType::Butt, {}});
    }
    return g;
}

ThreeLimbCoreMassRequest validRequest(
    const ThreeLimbCorePhysicalGeometry& geometry,
    const CoreMaterial& material)
{
    ThreeLimbCoreMassRequest request;
    request.geometry = &geometry;
    request.material = &material;
    request.stackingFactor = 0.95;
    request.dimensionalSource = "CORE-DRAWING";
    request.dimensionalRevision = "REV-C";
    request.materialRevisionReference = "core-revision-17#sha256";
    request.densitySourceReference = "manufacturer-datasheet:p2";
    request.stackingFactorSourceReference = "lamination-measurement:report-4";
    return request;
}

bool near(double a, double b, double tolerance = 1.0e-12)
{
    return std::abs(a - b) <= tolerance;
}

} // namespace

int main()
{
    CoreMaterial material;
    material.id = "TEST-CORE";
    material.density = units::Density{7650.0};

    auto geometry = validCore();
    auto request = validRequest(geometry, material);
    const auto result = calculateThreeLimbCoreMass(request);
    assert(result);
    assert(result.value().verificationStatus == "NOT_VERIFIED");

    const double limbVolume = 0.04 * 0.08 * 0.20 * 0.95;
    const double yokeVolume = 0.06 * 0.03 * 0.08 * 0.95;
    for (std::size_t i = 0; i < 3; ++i) {
        assert(near(result.value().netSteelVolumes[i].si(), limbVolume));
        assert(near(result.value().regionMasses[i].si(), limbVolume * 7650.0));
    }
    for (std::size_t i = 3; i < 7; ++i) {
        assert(near(result.value().netSteelVolumes[i].si(), yokeVolume));
        assert(near(result.value().regionMasses[i].si(), yokeVolume * 7650.0));
    }
    assert(near(result.value().totalNetSteelVolume.si(),
                3.0 * limbVolume + 4.0 * yokeVolume));
    assert(near(result.value().totalMass.si(),
                (3.0 * limbVolume + 4.0 * yokeVolume) * 7650.0));
    assert(result.value().regionMassSourceReferences[0].find("LIMB_OWNS_JOINTS") != std::string::npos);

    ThreeLimbCoreLossRequest operatingPoint;
    operatingPoint.frequency = units::Frequency{250.0};
    operatingPoint.peakFluxDensities.fill(units::FluxDensity{1.1});
    operatingPoint.temperatures.fill(units::TemperatureKelvin{333.15});
    const auto bound = bindThreeLimbCoreMass(operatingPoint, result.value());
    assert(bound);
    assert(near(bound.value().frequency.si(), 250.0));
    assert(near(bound.value().peakFluxDensities[4].si(), 1.1));
    assert(near(bound.value().temperatures[4].si(), 333.15));
    assert(near(bound.value().regionMasses[4].si(), result.value().regionMasses[4].si()));
    assert(bound.value().regionMassSourceReferences[4] ==
           result.value().regionMassSourceReferences[4]);

    auto invalidMassResult = result.value();
    invalidMassResult.regionMasses[2] = units::Mass{};
    assert(!bindThreeLimbCoreMass(operatingPoint, invalidMassResult));

    auto withGaps = validCore();
    withGaps.airGaps = {
        {CoreSegment::LimbA, units::Length{0.02}, units::Length{0.001}},
        {CoreSegment::YokeTopAB, units::Length{0.041}, units::Length{0.001}}
    };
    auto gapRequest = validRequest(withGaps, material);
    const auto gapped = calculateThreeLimbCoreMass(gapRequest);
    assert(gapped);
    const double limbGapVolume = 0.001 * 0.04 * 0.08 * 0.95;
    const double yokeGapVolume = 0.001 * 0.03 * 0.08 * 0.95;
    assert(near(gapped.value().netSteelVolumes[0].si(), limbVolume - limbGapVolume));
    assert(near(gapped.value().netSteelVolumes[3].si(), yokeVolume - yokeGapVolume));

    auto gapAcrossJoint = validCore();
    gapAcrossJoint.airGaps = {
        {CoreSegment::YokeTopAB, units::Length{0.039}, units::Length{0.002}}
    };
    auto invalidGapRequest = validRequest(gapAcrossJoint, material);
    assert(!calculateThreeLimbCoreMass(invalidGapRequest));

    auto stepLap = validCore();
    stepLap.joints[0].construction = CoreJointType::StepLap;
    stepLap.joints[0].stepLapPackets = {
        {units::Length{0.0}, units::Length{0.04}, units::Length{0.01},
         units::Length{0.01}, StepLapPacketDirection::LimbIntoYoke},
        {units::Length{0.04}, units::Length{0.04}, units::Length{0.01},
         units::Length{0.01}, StepLapPacketDirection::YokeIntoLimb}
    };
    auto stepLapRequest = validRequest(stepLap, material);
    assert(!calculateThreeLimbCoreMass(stepLapRequest));

    auto missingProvenance = validRequest(geometry, material);
    missingProvenance.densitySourceReference.clear();
    assert(!calculateThreeLimbCoreMass(missingProvenance));

    auto invalidDensity = material;
    invalidDensity.density = units::Density{};
    auto invalidDensityRequest = validRequest(geometry, invalidDensity);
    assert(!calculateThreeLimbCoreMass(invalidDensityRequest));

    auto invalidStacking = validRequest(geometry, material);
    invalidStacking.stackingFactor = 1.01;
    assert(!calculateThreeLimbCoreMass(invalidStacking));

    return 0;
}
