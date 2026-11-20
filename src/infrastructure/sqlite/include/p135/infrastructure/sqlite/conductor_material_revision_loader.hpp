#pragma once

#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"
#include "p135/domain/material_catalog.hpp"

#include <string>

namespace p135::infrastructure::sqlite {

struct ConductorMaterialRevisionSnapshot {
    domain::ConductorMaterial material;
    std::string revisionId;
    std::string sourceMaterialId;
    std::string contentHash;
    std::string sourceReference;
    std::string datasetRevision;
    std::string resistivityStatus;
    std::string designReleaseStatus;
    std::string notes;
};

/// Decodes an exact immutable conductor-material revision. No wire size,
/// conductor geometry, or insulation build is inferred from a product range.
[[nodiscard]] Result<ConductorMaterialRevisionSnapshot> loadConductorMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId);

} // namespace p135::infrastructure::sqlite
