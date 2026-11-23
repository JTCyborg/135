#pragma once

#include "p135/application/material_revision_use_cases.hpp"
#include "p135/domain/material_catalog.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <optional>
#include <string>
#include <utility>
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
    /// Source curve or estimated branch status; estimation never upgrades source quality.
    std::string bhCurveModelStatus;
    /// Original source anchors converted to B, kept separate from any estimated interpolation.
    std::vector<std::pair<double, double>> bhCurveSourceAnchors;
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
/// supported JSON schema to the domain material model. If no continuous
/// bhCurve is present, two or more generic magneticAnchors
/// [{fieldStrength_A_per_m, fluxDensity_T}] or
/// [{fieldStrength_A_per_m, polarization_T}] may generate an explicitly
/// estimated exploratory branch (B = J + mu0*H for polarization anchors).
/// The legacy material.minimumPolarization_T map is also supported. If no
/// anchors exist, material.initialRelativePermeability,
/// material.saturationPolarization_T and
/// material.modelMaximumFieldStrength_A_per_m generate a bounded
/// anhysteretic tanh saturation model. The model is not a hysteresis loop.
/// Sparse/parameter estimates never upgrade bhCurveStatus or
/// designReleaseStatus; a single anchor is preserved as insufficient rather
/// than extrapolated.
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
