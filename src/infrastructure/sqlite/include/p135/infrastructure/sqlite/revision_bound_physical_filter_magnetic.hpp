#pragma once

#include "p135/application/physical_filter_reactor_magnetic.hpp"
#include "p135/infrastructure/sqlite/core_material_revision_loader.hpp"

#include <string>

namespace p135::infrastructure::sqlite {

/// Result of a magnetic-cycle analysis bound to one exact immutable material revision.
struct RevisionBoundPhysicalFilterMagneticResult {
    CoreMaterialRevisionSnapshot materialRevision;
    application::PhysicalFilterReactorMagneticResult magneticAnalysis;
};

/// Loads the requested exact core-material revision, injects that material into
/// the physical filter request, and runs the magnetic-cycle analysis.
/// A caller-supplied material value is never trusted or used as a fallback.
[[nodiscard]] Result<RevisionBoundPhysicalFilterMagneticResult>
analyzePhysicalFilterReactorMagneticCycleWithMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& materialRevisionId,
    const application::PhysicalFilterReactorMagneticRequest& request);

} // namespace p135::infrastructure::sqlite
