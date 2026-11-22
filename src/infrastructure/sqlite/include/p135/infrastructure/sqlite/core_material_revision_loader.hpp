#pragma once

#include "p135/application/material_revision_use_cases.hpp"
#include "p135/domain/material_catalog.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace p135::infrastructure::sqlite {

struct CoreLossReferencePoint {
    units::Frequency frequency;
    units::FluxDensity peakFluxDensity;
    units::CoreLossDensity specificLoss;
    std::optional<units::TemperatureKelvin> measurementTemperature;
};

struct CoreLossSpecificationLimit {
    units::Frequency frequency;
    units::FluxDensity peakFluxDensity;
    units::CoreLossDensity maximumSpecificLoss;
    std::string limitType;
};

struct CoreLossGraphSource {
    std::string sourceDocument;
    std::string sourceReference;
    std::string graphScope;
    std::string captureStatus;
};

struct CoreLossReferenceDataset {
    std::string revisionId;
    std::string materialId;
    std::string manufacturer;
    std::string grade;
    std::string sourceReference;
    std::string datasetRevision;
    std::string contentHash;
    units::Length laminationThickness;
    units::Density density;
    double minimumStackingFactor{};
    std::vector<CoreLossReferencePoint> points;
    std::vector<CoreLossGraphSource> supplementaryGraphSources;
    std::optional<CoreLossSpecificationLimit> specificationLimit;
    std::string qualityStatus;
    std::string sourceDocument;
    std::string sourcePage;
    std::string measurementMethod;
    std::string sampleDirection;
    std::string waveform;
    std::string temperatureStatus;
    std::string notes;
};

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
    std::string coreLossSourceDocument;
    std::string coreLossSourcePage;
    std::string coreLossMeasurementMethod;
    std::string coreLossSampleDirection;
    std::string coreLossWaveform;
    std::string coreLossTemperatureStatus;
    std::string coreLossNotes;
    std::string designReleaseStatus;
    std::string notes;
    units::Frequency bhCurveFrequency;
    std::optional<units::TemperatureKelvin> bhCurveTemperature;
};

/// Loads one exact immutable revision (never "latest") and converts its
/// supported JSON schema to the domain material model. Unsupported or
/// incomplete datasets fail explicitly; no B-H or loss data is synthesized.
[[nodiscard]] Result<CoreMaterialRevisionSnapshot> loadCoreMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId);

/// Loads manufacturer-published AC loss reference anchors without forcing
/// unknown measurement temperature into the solver's CoreMaterial model.
[[nodiscard]] Result<CoreLossReferenceDataset> loadCoreLossReferenceDataset(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId);

/// Returns a manufacturer reference value only when both coordinates match
/// an imported anchor exactly. No interpolation, extrapolation, or temperature
/// inference is performed; dataset quality/provenance remains caller-visible.
[[nodiscard]] std::optional<CoreLossReferencePoint> findExactCoreLossReferencePoint(
    const CoreLossReferenceDataset& dataset,
    units::Frequency frequency,
    units::FluxDensity peakFluxDensity);

} // namespace p135::infrastructure::sqlite
