#pragma once

#include "p135/infrastructure/sqlite/conductor_material_revision_loader.hpp"
#include "p135/thermal/winding_electrothermal.hpp"

namespace p135::infrastructure::sqlite {

struct RevisionBoundWindingElectrothermalResult {
    ConductorMaterialRevisionSnapshot conductorMaterialRevision;
    thermal::WindingElectrothermalResult electrothermal;
};

// The exact immutable conductor revision replaces any material embedded in
// the request. Geometry, harmonic currents, cooling resistance and limits
// remain explicit inputs; no defaults are inferred.
[[nodiscard]] Result<RevisionBoundWindingElectrothermalResult>
analyzeRevisionBoundWindingElectrothermal(
    const SqliteMaterialRevisionStore& store,
    const std::string& conductorMaterialRevisionId,
    const thermal::WindingElectrothermalRequest& request);

} // namespace p135::infrastructure::sqlite
