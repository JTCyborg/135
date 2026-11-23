#pragma once

#include "p135/application/filter_reactor_magnetic.hpp"
#include "p135/domain/three_limb_geometry_adapter.hpp"

namespace p135::application {

/// Combines explicit CAD/physical core dimensions with the existing filter
/// harmonic-to-magnetic-cycle bridge. The supplied magnetic request must
/// already contain real winding turns and a pinned CoreMaterial revision.
struct PhysicalFilterReactorMagneticRequest {
    domain::ThreeLimbCorePhysicalGeometry physicalCore;
    double stackingFactor{};
    // Provenance for the physical dimensions used by this magnetic revision.
    std::string dimensionalSource;
    std::string dimensionalRevision;
    FilterReactorMagneticRequest filterMagnetic;
    // Material quality is populated from the exact immutable catalogue revision
    // before a stage artifact is accepted. Empty/default values fail closed.
    std::string bhCurveSourceStatus;
    std::string bhCurveModelStatus;
    std::string designReleaseStatus;
    // Explicit opt-in for exploratory analysis using a calculated B-H branch.
    // False by default; calculated material models are never design-verified.
    bool allowEstimatedBhModel{false};
};

struct PhysicalFilterReactorMagneticResult {
    domain::ThreeLimbMagneticGeometry effectiveMagneticGeometry;
    FilterReactorMagneticResult magneticAnalysis;
};

/// Converts the explicit physical core geometry and runs the selected
/// filter branch through the nonlinear magnetic-cycle solver.
/// No core dimensions, turns, material data or harmonic currents are inferred.
[[nodiscard]] Result<PhysicalFilterReactorMagneticResult>
analyzePhysicalFilterReactorMagneticCycle(
    const PhysicalFilterReactorMagneticRequest& request);

} // namespace p135::application
