#pragma once

#include "p135/core/result.hpp"
#include "p135/domain/mechanical_geometry.hpp"

#include <string>

namespace p135::cad {

// Generates a three-view DXF drawing from the same explicit physical geometry
// used by the 3D adapter. Dimensions are written in millimetres (DXF INSUNITS=4).
// The drawing is PRELIMINARY and carries the source/revision provenance; it is
// not a manufacturing release or an engineering approval.
[[nodiscard]] Result<void> exportReactorAssemblyDxf(
    const domain::ReactorPhysicalGeometry& geometry,
    const std::string& filePath);

} // namespace p135::cad
