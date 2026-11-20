#pragma once

#include "p135/application/material_revision_use_cases.hpp"
#include "p135/domain/material_catalog.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <string>

namespace p135::infrastructure::sqlite {

/// Engineering quality/provenance information that is not part of the solver's
/// CoreMaterial value object but must travel with the decoded immutable revision.
struct CoreMaterialRevisionSnapshot {
    domain::CoreMaterial material;
    std::string revisionId;
    std::string sourceMaterialId;
    std::string contentHash;
    std::string sourceReference;
    std::string datasetRevision;
    std::string bhCurveStatus;
    std::string bhCurveSourceDocument;
    std::string bhCurveSourcePage;
    std::string bhCurveMeasurementMethod;
    std::string bhCurveSampleDirection;
    std::string bhCurveCaptureMethod;
    std::string bhCurveNotes;
    std::string coreLossStatus;
    std::string designReleaseStatus;
    std::string notes;
    units::Frequency bhCurveFrequency;
    units::TemperatureKelvin bhCurveTemperature;
};

/// Loads one exact immutable revision (never "latest") and converts its
/// supported JSON schema to the domain material model. Unsupported or
/// incomplete datasets fail explicitly; no B-H or loss data is synthesized.
[[nodiscard]] Result<CoreMaterialRevisionSnapshot> loadCoreMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId);

} // namespace p135::infrastructure::sqlite
