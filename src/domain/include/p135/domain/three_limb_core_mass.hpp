#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/magnetic_product.hpp"
#include "p135/domain/mechanical_geometry.hpp"

#include <array>
#include <string>

namespace p135::domain {

/// Region order matches ThreeLimbCoreLossRequest:
/// limb A, limb B, limb C, top yoke AB, top yoke BC, bottom yoke AB, bottom yoke BC.
struct ThreeLimbCoreMassRequest {
    const ThreeLimbCorePhysicalGeometry* geometry{};
    const CoreMaterial* material{};
    double stackingFactor{};
    std::string dimensionalSource;
    std::string dimensionalRevision;
    std::string materialRevisionReference;
    std::string densitySourceReference;
    std::string stackingFactorSourceReference;
};

struct ThreeLimbCoreMassResult {
    std::array<units::Volume, 7> netSteelVolumes{};
    std::array<units::Mass, 7> regionMasses{};
    std::array<std::string, 7> regionMassSourceReferences{};
    units::Volume totalNetSteelVolume;
    units::Mass totalMass;
    /// Always NOT_VERIFIED: valid arithmetic and provenance strings do not
    /// independently authenticate source documents, density or stacking factor.
    std::string verificationStatus{"NOT_VERIFIED"};
    std::string volumePartitionMethod{
        "LIMB_OWNS_JOINTS; YOKES_COUNT_CLEAR_WINDOWS_ONLY; AIR_GAPS_SUBTRACTED"};
};

/// Calculates non-overlapping material volumes for the seven magnetic regions.
/// Butt-joint volumes are partitioned by assigning all limb/yoke intersection
/// material to the limb and only clear-window material to each yoke region.
/// Step-Lap is rejected until packet-resolved CAD union volumes can be used;
/// this avoids inventing or double-counting interleaved joint steel.
[[nodiscard]] Result<ThreeLimbCoreMassResult> calculateThreeLimbCoreMass(
    const ThreeLimbCoreMassRequest& request);

} // namespace p135::domain
