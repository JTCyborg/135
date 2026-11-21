#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace p135::domain {

enum class CoreJointType { Butt, StepLap };
enum class StepLapPacketDirection { LimbIntoYoke, YokeIntoLimb };
enum class CoreJointInterface {
    TopYokeABToLimbA,
    TopYokeABToLimbB,
    TopYokeBCToLimbB,
    TopYokeBCToLimbC,
    BottomYokeABToLimbA,
    BottomYokeABToLimbB,
    BottomYokeBCToLimbB,
    BottomYokeBCToLimbC
};
enum class CoreSegment { LimbA, LimbB, LimbC, YokeTopAB, YokeTopBC, YokeBottomAB, YokeBottomBC };

// A measured packet occupies an explicit slice of the common core stack-depth
// datum. Overlap lengths are measured from the nominal butt plane into each
// named member; they are not inferred from magnetic path dimensions.
struct StepLapPacketPhysicalGeometry {
    units::Length stackDepthOffset;
    units::Length stackDepth;
    units::Length overlapIntoLimb;
    units::Length overlapIntoYoke;
    StepLapPacketDirection direction{StepLapPacketDirection::LimbIntoYoke};
};

struct CoreJointPhysicalGeometry {
    CoreJointInterface interface{CoreJointInterface::TopYokeABToLimbA};
    CoreJointType construction{CoreJointType::Butt};
    std::vector<StepLapPacketPhysicalGeometry> stepLapPackets;
};

struct AirGapPhysicalGeometry {
    CoreSegment segment{CoreSegment::LimbA};
    // Distance from the lower end of a limb or the left end of a yoke segment.
    units::Length positionFromSegmentStart;
    units::Length length;
};

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
    std::vector<CoreJointPhysicalGeometry> joints; // all eight limb/yoke interfaces, explicitly specified
    std::vector<AirGapPhysicalGeometry> airGaps; // zero or more explicitly positioned cuts
    std::string laminationConstruction; // e.g. packet/step-lap description; no implicit default
};

// Three-limb reactor CAD datum: X is limb width, Y is the limb/bobbin
// axis, and Z is the core stack-depth direction. Consequently the bobbin
// opening is measured in the X-Z plane; axialLength is along Y.
struct BobbinPhysicalGeometry {
    units::Length innerWidth;  // clear opening along X
    units::Length innerHeight; // clear opening along Z
    units::Length axialLength;  // bobbin length along Y
    units::Length wallThickness;
    units::Length flangeThickness;
    units::Length flangeOuterWidth;  // explicit flange envelope along X
    units::Length flangeOuterHeight; // explicit flange envelope along Z
    units::Length windingClearance;
    std::string material;
};

// Winding axialHeight is measured along the bobbin Y axis. endClearance is
// the required free distance at each axial end inside the bobbin. radialBuild
// remains a separately supplied construction dimension; its detailed
// distribution around X/Z must be defined before winding solids are generated.
struct WindingPackPhysicalGeometry {
    units::Length radialBuild;
    units::Length axialHeight;
    units::Length endClearance;
    units::Length layerInsulationBuild;
    units::Length mainInsulationBuild;
    units::Length conductorInsulationBuild;
    std::uint32_t layerCount{};
    std::uint32_t sections{};
    // Optional conductor-level construction inputs. When supplied, the set
    // is validated as a whole; absent values are never synthesized by CAD.
    std::optional<std::uint32_t> turns;
    std::optional<std::uint32_t> turnsPerLayer;
    std::optional<std::uint32_t> parallelConductors;
    std::string conductorShape;
    std::optional<units::Length> bareConductorDiameter;
    std::optional<units::Length> insulatedConductorDiameter;
    std::string constructionDescription;
};

struct ReactorPhysicalGeometry {
    std::string productId;
    // Pinned catalog revisions are presentation provenance as well as design inputs.
    // Empty means no explicit revision was selected; consumers must not infer one.
    std::string coreMaterialRevisionId;
    std::string conductorMaterialRevisionId;
    std::string insulationMaterialRevisionId;
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
