#pragma once

#include "p135/domain/mechanical_geometry.hpp"
#include "p135/domain/three_limb_magnetic.hpp"

namespace p135::domain {

/// Calculates the net magnetic cross-section of a laminated limb from
/// explicit gross dimensions and a supplied stacking factor.
[[nodiscard]] Result<units::Area> calculateLaminatedLimbEffectiveArea(
    units::Length limbWidth, units::Length stackDepth, double stackingFactor);

/// Converts explicitly supplied three-limb physical dimensions into the
/// effective branch geometry consumed by the nonlinear magnetic solver.
/// Areas include the declared stacking factor. No dimensions are synthesized.
[[nodiscard]] Result<ThreeLimbMagneticGeometry> buildThreeLimbMagneticGeometry(
    const ThreeLimbCorePhysicalGeometry& physical,
    double stackingFactor);

} // namespace p135::domain
