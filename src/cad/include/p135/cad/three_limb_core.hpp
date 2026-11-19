#pragma once

#include "p135/core/result.hpp"
#include "p135/domain/mechanical_geometry.hpp"

#include <TopoDS_Shape.hxx>

namespace p135::cad {

// Builds a seven-component B-Rep compound (three limbs and four yoke segments)
// from explicit physical dimensions. The returned shape is not a fused single
// solid; assembly identity is retained for the later BOM/assembly stage.
[[nodiscard]] Result<TopoDS_Shape> buildThreeLimbCoreShape(
    const domain::ReactorPhysicalGeometry& geometry);

} // namespace p135::cad
