#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace p135::domain {

enum class CoreJointType { Butt, StepLap, DistributedGap, Custom };
enum class AirGapLocation { LimbA, LimbB, LimbC, YokeTopAB, YokeTopBC, YokeBottomAB, YokeBottomBC, Multiple, None };

// Physical dimensions used by CAD. These are deliberately distinct from
// effective magnetic Ae/Aw/le values in CoreGeometry and ThreeLimbMagneticGeometry.
// All lengths are SI metres; conversion to CAD millimetres occurs in the CAD adapter.
struct ThreeLimbCorePhysicalGeometry {
    std::array<units::Length, 3> limbWidths{};       // X section, A/B/C
    std::array<units::Length, 3> limbDepths{};       // stack/depth per limb
    std::array<units::Length, 2> windowWidths{};     // left and right clear windows
    std::array<units::Length, 2> windowHeights{};
    std::array<units::Length, 4> yokeThicknesses{};  // top A/B, top B/C, bottom A/B, bottom B/C
    std::array<units::Length, 4> yokeDepths{};
    std::array<units::Length, 3> limbCenterSpacings{}; // A-B, B-C, overall A-C
    units::Length coreOverallHeight;
    units::Length coreOverallWidth;
    CoreJointType jointType{CoreJointType::Custom};
    AirGapLocation airGapLocation{AirGapLocation::None};
    std::array<units::Length, 7> airGapLengths{}; // A/B/C limbs then four yoke segments
    std::string laminationConstruction; // e.g. packet/step-lap description; no implicit default
};

struct BobbinPhysicalGeometry {
    units::Length innerWidth;
    units::Length innerHeight;
    units::Length axialLength;
    units::Length wallThickness;
    units::Length flangeThickness;
    units::Length windingClearance;
    std::string material;
};

struct WindingPackPhysicalGeometry {
    units::Length radialBuild;
    units::Length axialHeight;
    units::Length endClearance;
    units::Length layerInsulationBuild;
    units::Length mainInsulationBuild;
    units::Length conductorInsulationBuild;
    std::uint32_t layerCount{};
    std::uint32_t sections{};
    std::string constructionDescription;
};

struct ReactorPhysicalGeometry {
    std::string productId;
    ThreeLimbCorePhysicalGeometry core;
    std::array<BobbinPhysicalGeometry, 3> bobbins{};
    std::array<WindingPackPhysicalGeometry, 3> windingPacks{};
    std::string dimensionalSource; // drawing, supplier datasheet or measured reference
    std::string dimensionalRevision;
};

// Validates completeness and basic geometric consistency only. It does not
// certify insulation coordination, thermal limits, manufacturability or standards.
[[nodiscard]] Result<void> validate(const ThreeLimbCorePhysicalGeometry& geometry);
[[nodiscard]] Result<void> validate(const BobbinPhysicalGeometry& geometry);
[[nodiscard]] Result<void> validate(const WindingPackPhysicalGeometry& geometry);
[[nodiscard]] Result<void> validate(const ReactorPhysicalGeometry& geometry);

} // namespace p135::domain
