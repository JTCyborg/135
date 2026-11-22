#include "p135/infrastructure/sqlite/core_material_revision_loader.hpp"

#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <stdexcept>
#include <exception>
#include <utility>

namespace p135::infrastructure::sqlite {
namespace {

using nlohmann::json;

Error loaderError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CORE_MATERIAL_REVISION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

domain::CoreMaterialFamily parseFamily(const std::string& family)
{
    if (family == "MnZn ferrite" || family == "ferrite")
        return domain::CoreMaterialFamily::Ferrite;
    if (family == "CRGO" || family == "grain-oriented electrical steel")
        return domain::CoreMaterialFamily::CRGO;
    if (family == "electrical steel" ||
        family == "non-grain-oriented electrical steel" ||
        family == "fully finished non-grain-oriented electrical steel")
        return domain::CoreMaterialFamily::ElectricalSteel;
    if (family == "amorphous Fe-based ribbon")
        return domain::CoreMaterialFamily::Amorphous;
    if (family == "nanocrystalline")
        return domain::CoreMaterialFamily::Nanocrystalline;
    if (family == "powder")
        return domain::CoreMaterialFamily::Powder;
    throw std::invalid_argument("Unsupported core material family: " + family);
}

double requiredNumber(const json& object, const char* key)
{
    const auto& value = object.at(key);
    if (!value.is_number())
        throw std::invalid_argument(std::string("Expected numeric field: ") + key);
    const double number = value.get<double>();
    if (!std::isfinite(number))
        throw std::invalid_argument(std::string("Non-finite numeric field: ") + key);
    return number;
}

std::string requiredString(const json& object, const char* key)
{
    const auto& value = object.at(key);
    if (!value.is_string())
        throw std::invalid_argument(std::string("Expected string field: ") + key);
    const auto result = value.get<std::string>();
    if (result.empty())
        throw std::invalid_argument(std::string("Empty string field: ") + key);
    return result;
}

std::string provenanceString(const json& object, const char* key)
{
    if (!object.is_object() || !object.contains(key) || !object.at(key).is_string())
        return "NOT_PROVIDED";
    const auto value = object.at(key).get<std::string>();
    return value.empty() ? "NOT_PROVIDED" : value;
}

} // namespace

Result<CoreMaterialRevisionSnapshot> loadCoreMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId)
{
    if (revisionId.empty())
        return Result<CoreMaterialRevisionSnapshot>::failure(loaderError(
            ErrorCode::MissingRequiredData, "An exact core material revision ID is required.",
            "revisionId"));

    const auto stored = store.findRevision(revisionId);
    if (!stored)
        return Result<CoreMaterialRevisionSnapshot>::failure(stored.error());
    if (!stored.value())
        return Result<CoreMaterialRevisionSnapshot>::failure(loaderError(
            ErrorCode::MissingRequiredData, "The requested material revision does not exist.",
            "revisionId"));

    const auto& revision = *stored.value();
    if (revision.recordType != "core-material")
        return Result<CoreMaterialRevisionSnapshot>::failure(loaderError(
            ErrorCode::InvalidInput, "The requested revision is not a core-material record.",
            "recordType"));

    const auto canonical = canonicalizeJson(revision.payload);
    if (!canonical)
        return Result<CoreMaterialRevisionSnapshot>::failure(canonical.error());
    if (canonical.value() != revision.payload)
        return Result<CoreMaterialRevisionSnapshot>::failure(loaderError(
            ErrorCode::InvalidInput, "Stored material payload is not canonical JSON.", "payload"));
    const auto hash = sha256Hex(revision.payload);
    if (!hash)
        return Result<CoreMaterialRevisionSnapshot>::failure(hash.error());
    if (hash.value() != revision.contentHash)
        return Result<CoreMaterialRevisionSnapshot>::failure(loaderError(
            ErrorCode::InvalidInput, "Stored material payload hash does not match its revision.",
            "contentHash"));

    try {
        const auto root = json::parse(revision.payload);
        if (!root.is_object() || requiredNumber(root, "schemaVersion") != 1.0)
            throw std::invalid_argument("Unsupported core material payload schema version.");

        const auto& sourceMaterial = root.at("material");
        domain::CoreMaterial material;
        const std::string sourceMaterialId = requiredString(sourceMaterial, "id");
        // The catalogue's stable identity (MAT-...) is intentionally distinct
        // from the manufacturer's grade/product identifier stored in payload.
        // Use the immutable revision identity in the domain model and preserve
        // the source identifier separately for traceability.
        material.id = revision.materialId;
        material.manufacturer = requiredString(sourceMaterial, "manufacturer");
        material.grade = requiredString(sourceMaterial, "grade");
        material.family = parseFamily(requiredString(sourceMaterial, "family"));
        material.density = units::Density{requiredNumber(sourceMaterial, "density_kg_per_m3")};
        const auto& quality = root.at("dataQuality");
        const std::string sourceStatus = provenanceString(quality, "sourceStatus");
        material.dataSource = sourceStatus == "MEASURED_RESEARCH_DATASET"
            ? domain::MaterialDataSource::Measurement
            : domain::MaterialDataSource::ManufacturerDatasheet;
        material.sourceReference = revision.sourceReference;
        material.datasetRevision = revision.datasetRevision;

        // Sparse magnetic anchors can be converted to an explicitly estimated
        // exploratory B-H branch. This never changes the source quality status.
        const auto bhIt = root.find("bhCurve");
        double bhFrequency = 0.0;
        double bhTemperature = 0.0;
        std::string bhCurveModelStatus = bhIt != root.end()
            ? "SOURCE_CURVE_PRESENT" : "NO_SOURCE_BH_CURVE";
        if (bhIt == root.end()) {
            std::vector<std::pair<double, double>> anchors;
            constexpr double mu0 = 4.0e-7 * 3.14159265358979323846;
            const auto generic = root.find("magneticAnchors");
            if (generic != root.end() && generic->is_array()) {
                if (root.contains("magneticAnchorFrequency_Hz"))
                    bhFrequency = requiredNumber(root, "magneticAnchorFrequency_Hz");
                if (root.contains("magneticAnchorTemperature_K"))
                    bhTemperature = requiredNumber(root, "magneticAnchorTemperature_K");
                for (const auto& anchor : *generic) {
                    const double h = requiredNumber(anchor, "fieldStrength_A_per_m");
                    double b = 0.0;
                    if (anchor.contains("fluxDensity_T"))
                        b = requiredNumber(anchor, "fluxDensity_T");
                    else if (anchor.contains("polarization_T"))
                        b = requiredNumber(anchor, "polarization_T") + mu0 * h;
                    else
                        throw std::invalid_argument("A magnetic anchor requires fluxDensity_T or polarization_T.");
                    anchors.emplace_back(h, b);
                }
            } else {
                const auto values = sourceMaterial.find("minimumPolarization_T");
                if (values != sourceMaterial.end() && values->is_object()) {
                    if (sourceMaterial.contains("minimumPolarizationFrequency_Hz"))
                        bhFrequency = requiredNumber(sourceMaterial, "minimumPolarizationFrequency_Hz");
                    for (auto it = values->begin(); it != values->end(); ++it) {
                        const std::string key = it.key();
                        const std::string prefix = "at_";
                        const std::string suffix = "_A_per_m";
                        if (key.rfind(prefix, 0) != 0 ||
                            key.size() <= prefix.size() + suffix.size() ||
                            key.substr(key.size() - suffix.size()) != suffix ||
                            !it.value().is_number())
                            throw std::invalid_argument("Invalid minimum-polarization anchor: " + key);
                        const double h = std::stod(key.substr(prefix.size(),
                            key.size() - prefix.size() - suffix.size()));
                        const double j = it.value().get<double>();
                        if (!std::isfinite(h) || !std::isfinite(j) || h <= 0.0 || j <= 0.0)
                            throw std::invalid_argument("Minimum-polarization anchors must be finite and positive.");
                        anchors.emplace_back(h, j + mu0 * h);
                    }
                }
            }
            if (anchors.size() == 1) {
                bhCurveModelStatus = "INSUFFICIENT_MAGNETIC_ANCHORS";
            }
            if (anchors.size() >= 2) {
                std::sort(anchors.begin(), anchors.end(),
                    [](const auto& left, const auto& right) { return left.first < right.first; });
                material.bhCurve.push_back({units::MagneticFieldStrength{0.0},
                                            units::FluxDensity{0.0}});
                double h0 = 0.0;
                double b0 = 0.0;
                for (const auto& [h1, b1] : anchors) {
                    if (!std::isfinite(h1) || !std::isfinite(b1) ||
                        h1 <= h0 || b1 <= b0)
                        throw std::invalid_argument("Magnetic anchors must increase strictly in H and B.");
                    // Subdivide each source interval for stable solver input and
                    // a useful plotted curve; subdivisions add no new evidence.
                    for (int n = 1; n <= 16; ++n) {
                        const double t = static_cast<double>(n) / 16.0;
                        material.bhCurve.push_back({
                            units::MagneticFieldStrength{h0 + t * (h1 - h0)},
                            units::FluxDensity{b0 + t * (b1 - b0)}
                        });
                    }
                    h0 = h1;
                    b0 = b1;
                }
                bhCurveModelStatus = "ESTIMATED_FROM_SPARSE_MAGNETIC_ANCHORS";
            }
            if (anchors.size() < 2) {
                // Optional anhysteretic saturation model for materials whose
                // datasheet gives initial relative permeability and saturation
                // polarization but no tabulated B-H points. Hmax is mandatory:
                // the loader never invents the model's operating domain.
                const bool hasParameterModel =
                    sourceMaterial.contains("initialRelativePermeability") &&
                    sourceMaterial.contains("saturationPolarization_T") &&
                    sourceMaterial.contains("modelMaximumFieldStrength_A_per_m");
                if (hasParameterModel) {
                    const double muR = requiredNumber(sourceMaterial, "initialRelativePermeability");
                    const double js = requiredNumber(sourceMaterial, "saturationPolarization_T");
                    const double hMax = requiredNumber(sourceMaterial, "modelMaximumFieldStrength_A_per_m");
                    if (muR <= 1.0 || js <= 0.0 || hMax <= 0.0)
                        throw std::invalid_argument(
                            "Saturation model requires mu_r_initial > 1, J_s > 0 and H_max > 0.");
                    if (sourceMaterial.contains("parameterModelFrequency_Hz"))
                        bhFrequency = requiredNumber(sourceMaterial, "parameterModelFrequency_Hz");
                    if (sourceMaterial.contains("parameterModelTemperature_K"))
                        bhTemperature = requiredNumber(sourceMaterial, "parameterModelTemperature_K");
                    const double hScale = js / (mu0 * (muR - 1.0));
                    if (!std::isfinite(hScale) || hScale <= 0.0)
                        throw std::invalid_argument("Invalid characteristic field derived from saturation parameters.");
                    constexpr int samples = 128;
                    material.bhCurve.reserve(samples + 1);
                    for (int n = 0; n <= samples; ++n) {
                        const double h = hMax * static_cast<double>(n) / samples;
                        const double polarization = js * std::tanh(h / hScale);
                        const double b = mu0 * h + polarization;
                        material.bhCurve.push_back({
                            units::MagneticFieldStrength{h},
                            units::FluxDensity{b}
                        });
                    }
                    bhCurveModelStatus = "ESTIMATED_FROM_SATURATION_PARAMETERS";
                }
            }
        }
        if (bhIt != root.end()) {
            const auto& sourceBh = *bhIt;
            bhFrequency = requiredNumber(sourceBh, "frequency_Hz");
            bhTemperature = requiredNumber(sourceBh, "temperature_K");
            const auto pointsIt = sourceBh.find("points");
            if (pointsIt == sourceBh.end() || !pointsIt->is_array() ||
                pointsIt->size() < 2)
                throw std::invalid_argument("At least two source B-H points are required when a B-H curve is supplied.");
            for (const auto& point : *pointsIt) {
                material.bhCurve.push_back(domain::BHPoint{
                    units::MagneticFieldStrength{requiredNumber(point, "fieldStrength_A_per_m")},
                    units::FluxDensity{requiredNumber(point, "fluxDensity_T")}
                });
            }
        }

        const json sourceLoss = root.value("coreLossData", json::object());
        const json lossPoints = sourceLoss.value("points", json::array());
        if (!lossPoints.is_array())
            throw std::invalid_argument("Core-loss points must be an array when supplied.");
        material.lossDataReference = revision.sourceReference;
        for (const auto& point : lossPoints) {
            // Starter records publish volumetric loss (W/m^3), while the
            // domain CoreLossPoint stores mass-specific loss (W/kg).
            // Convert using the same revision's published material density.
            const double volumetricLoss = requiredNumber(point, "specificLoss_W_per_m3");
            const double massSpecificLoss = volumetricLoss / material.density.si();
            material.lossData.push_back(domain::CoreLossPoint{
                units::Frequency{requiredNumber(point, "frequency_Hz")},
                units::FluxDensity{requiredNumber(point, "peakFluxDensity_T")},
                units::TemperatureKelvin{requiredNumber(point, "temperature_K")},
                units::CoreLossDensity{massSpecificLoss}
            });
        }

        CoreMaterialRevisionSnapshot snapshot;
        snapshot.material = std::move(material);
        snapshot.revisionId = revision.revisionId;
        snapshot.sourceMaterialId = sourceMaterialId;
        snapshot.contentHash = revision.contentHash;
        snapshot.sourceReference = revision.sourceReference;
        snapshot.datasetRevision = revision.datasetRevision;
        snapshot.bhCurveStatus = requiredString(quality, "bhCurveStatus");
        snapshot.bhCurveModelStatus = bhCurveModelStatus;
        const json bhProvenance = root.value("bhCurveProvenance", json::object());
        // Older immutable revisions predate the provenance object. Keep them
        // readable while explicitly exposing missing provenance to callers.
        snapshot.bhCurveSourceDocument = provenanceString(bhProvenance, "sourceDocument");
        snapshot.bhCurveSourcePage = provenanceString(bhProvenance, "sourcePage");
        snapshot.bhCurveMeasurementMethod = provenanceString(bhProvenance, "measurementMethod");
        snapshot.bhCurveSampleDirection = provenanceString(bhProvenance, "sampleDirection");
        snapshot.bhCurveCaptureMethod = provenanceString(bhProvenance, "captureMethod");
        snapshot.bhCurveNotes = provenanceString(bhProvenance, "notes");
        snapshot.coreLossStatus = requiredString(quality, "coreLossStatus");
        const json lossProvenance = root.value("coreLossProvenance", json::object());
        snapshot.coreLossSourceDocument = provenanceString(lossProvenance, "sourceDocument");
        snapshot.coreLossSourcePage = provenanceString(lossProvenance, "sourcePage");
        snapshot.coreLossMeasurementMethod = provenanceString(lossProvenance, "measurementMethod");
        snapshot.coreLossSampleDirection = provenanceString(lossProvenance, "sampleDirection");
        snapshot.coreLossWaveform = provenanceString(lossProvenance, "waveform");
        snapshot.coreLossTemperatureStatus = provenanceString(lossProvenance, "temperatureStatus");
        snapshot.coreLossNotes = provenanceString(lossProvenance, "notes");
        snapshot.designReleaseStatus = requiredString(quality, "designReleaseStatus");
        snapshot.notes = requiredString(quality, "notes");
        if (bhIt != root.end()) {
            const std::string bhCurveType = requiredString(*bhIt, "curveType");
            if (bhCurveType == "DC initial magnetization curve") {
                if (bhFrequency != 0.0)
                    throw std::invalid_argument("A DC B-H curve must declare frequency_Hz as zero.");
            } else if (bhFrequency <= 0.0) {
                throw std::invalid_argument("A non-DC B-H curve must declare a positive frequency.");
            }
        }
        snapshot.bhCurveFrequency = units::Frequency{bhFrequency};
        snapshot.bhCurveTemperature = bhTemperature > 0.0
            ? std::optional<units::TemperatureKelvin>{units::TemperatureKelvin{bhTemperature}}
            : std::nullopt;

        if (!snapshot.material.bhCurve.empty()) {
            const auto validated = domain::validate(snapshot.material);
            if (!validated)
                return Result<CoreMaterialRevisionSnapshot>::failure(validated.error());
        } else if (!std::isfinite(snapshot.material.density.si()) ||
                   snapshot.material.density.si() <= 0.0) {
            return Result<CoreMaterialRevisionSnapshot>::failure(loaderError(
                ErrorCode::MissingMaterialProperty,
                "Material density must be positive even when no B-H curve is published.",
                "material.density"));
        }
        if (bhTemperature < 0.0)
            throw std::invalid_argument("B-H curve temperature cannot be negative.");
        return Result<CoreMaterialRevisionSnapshot>::success(std::move(snapshot));
    } catch (const std::exception& exception) {
        return Result<CoreMaterialRevisionSnapshot>::failure(loaderError(
            ErrorCode::InvalidInput,
            std::string("Cannot decode core material revision: ") + exception.what(),
            "payload"));
    }
}


Result<CoreLossReferenceDataset> loadCoreLossReferenceDataset(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId)
{
    if (revisionId.empty())
        return Result<CoreLossReferenceDataset>::failure(loaderError(
            ErrorCode::MissingRequiredData, "An exact core-loss dataset revision ID is required.",
            "revisionId"));

    const auto stored = store.findRevision(revisionId);
    if (!stored)
        return Result<CoreLossReferenceDataset>::failure(stored.error());
    if (!stored.value())
        return Result<CoreLossReferenceDataset>::failure(loaderError(
            ErrorCode::MissingRequiredData, "The requested core-loss dataset does not exist.",
            "revisionId"));

    const auto& revision = *stored.value();
    if (revision.recordType != "core-loss-reference")
        return Result<CoreLossReferenceDataset>::failure(loaderError(
            ErrorCode::InvalidInput, "The requested revision is not a core-loss-reference record.",
            "recordType"));

    const auto canonical = canonicalizeJson(revision.payload);
    if (!canonical)
        return Result<CoreLossReferenceDataset>::failure(canonical.error());
    if (canonical.value() != revision.payload)
        return Result<CoreLossReferenceDataset>::failure(loaderError(
            ErrorCode::InvalidInput, "Stored core-loss payload is not canonical JSON.", "payload"));
    const auto hash = sha256Hex(revision.payload);
    if (!hash)
        return Result<CoreLossReferenceDataset>::failure(hash.error());
    if (hash.value() != revision.contentHash)
        return Result<CoreLossReferenceDataset>::failure(loaderError(
            ErrorCode::InvalidInput, "Stored core-loss payload hash does not match its revision.",
            "contentHash"));

    try {
        const auto root = json::parse(revision.payload);
        if (!root.is_object() || requiredNumber(root, "schemaVersion") != 1.0)
            throw std::invalid_argument("Unsupported core-loss reference schema version.");
        const auto& material = root.at("material");
        CoreLossReferenceDataset dataset;
        dataset.revisionId = revision.revisionId;
        dataset.materialId = requiredString(material, "id");
        dataset.manufacturer = requiredString(material, "manufacturer");
        dataset.grade = requiredString(material, "grade");
        dataset.sourceReference = revision.sourceReference;
        dataset.datasetRevision = revision.datasetRevision;
        dataset.contentHash = revision.contentHash;
        if (dataset.sourceReference.empty() || dataset.datasetRevision.empty())
            throw std::invalid_argument("Core-loss reference dataset provenance must include sourceReference and datasetRevision.");
        dataset.laminationThickness = units::Length{requiredNumber(material, "laminationThickness_m")};
        dataset.density = units::Density{requiredNumber(material, "density_kg_per_m3")};
        dataset.minimumStackingFactor = requiredNumber(material, "minimumLaminationFactor");
        if (dataset.laminationThickness.si() <= 0.0 ||
            dataset.density.si() <= 0.0 ||
            dataset.minimumStackingFactor <= 0.0 ||
            dataset.minimumStackingFactor > 1.0)
            throw std::invalid_argument("Core-loss dataset material properties are outside valid ranges.");

        const auto& points = root.at("coreLossReferencePoints");
        if (!points.is_array() || points.empty())
            throw std::invalid_argument("At least one AC core-loss reference point is required.");
        std::set<std::pair<double, double>> coordinates;
        for (const auto& point : points) {
            const double frequency = requiredNumber(point, "frequency_Hz");
            const double flux = requiredNumber(point, "peakFluxDensity_T");
            const double loss = requiredNumber(point, "specificLoss_W_per_kg");
            const bool hasTemperature = point.contains("temperature_K");
            const double temperature = hasTemperature
                ? requiredNumber(point, "temperature_K")
                : 0.0;
            if (frequency <= 0.0 || flux <= 0.0 || loss < 0.0 ||
                (hasTemperature && temperature <= 0.0))
                throw std::invalid_argument("AC core-loss reference point contains an invalid value.");
            if (!coordinates.emplace(frequency, flux).second)
                throw std::invalid_argument(
                    "AC core-loss reference dataset contains duplicate frequency/flux coordinates.");
            dataset.points.push_back(CoreLossReferencePoint{
                units::Frequency{frequency},
                units::FluxDensity{flux},
                units::CoreLossDensity{loss},
                hasTemperature
                    ? std::optional<units::TemperatureKelvin>{units::TemperatureKelvin{temperature}}
                    : std::nullopt
            });
        }

        const json graphSources = root.value("supplementaryGraphSources", json::array());
        if (!graphSources.is_array())
            throw std::invalid_argument("Supplementary graph sources must be an array when supplied.");
        for (const auto& source : graphSources) {
            CoreLossGraphSource graph;
            graph.sourceDocument = requiredString(source, "sourceDocument");
            graph.sourceReference = requiredString(source, "sourceReference");
            graph.graphScope = requiredString(source, "graphScope");
            graph.captureStatus = requiredString(source, "captureStatus");
            if (graph.sourceReference.rfind("https://", 0) != 0)
                throw std::invalid_argument("Supplementary graph source must use an HTTPS reference.");
            if (graph.captureStatus != "GRAPH_ONLY_NOT_DIGITIZED" &&
                graph.captureStatus != "APPROXIMATE_DIGITIZATION_NOT_VALIDATED")
                throw std::invalid_argument("Unsupported supplementary graph capture status.");
            dataset.supplementaryGraphSources.push_back(std::move(graph));
        }

        const json limits = root.value("coreLossSpecificationLimits", json::object());
        if (limits.contains("maximumSpecificLoss_W_per_kg")) {
            const auto& limit = limits.at("maximumSpecificLoss_W_per_kg");
            const double frequency = requiredNumber(limit, "frequency_Hz");
            const double flux = requiredNumber(limit, "peakFluxDensity_T");
            const double value = requiredNumber(limit, "value");
            if (frequency <= 0.0 || flux <= 0.0 || value < 0.0)
                throw std::invalid_argument("Core-loss specification limit contains an invalid value.");
            dataset.specificationLimit = CoreLossSpecificationLimit{
                units::Frequency{frequency},
                units::FluxDensity{flux},
                units::CoreLossDensity{value},
                requiredString(limit, "limitType")
            };
        }

        const json provenance = root.value("coreLossProvenance", json::object());
        const json quality = root.value("dataQuality", json::object());
        dataset.qualityStatus = provenanceString(quality, "coreLossStatus");
        dataset.sourceDocument = provenanceString(provenance, "sourceDocument");
        dataset.sourcePage = provenanceString(provenance, "sourcePage");
        dataset.measurementMethod = provenanceString(provenance, "measurementMethod");
        dataset.sampleDirection = provenanceString(provenance, "sampleDirection");
        dataset.waveform = provenanceString(provenance, "waveform");
        dataset.temperatureStatus = provenanceString(provenance, "temperatureStatus");
        dataset.notes = provenanceString(provenance, "notes");
        if (dataset.qualityStatus == "NOT_PROVIDED" ||
            dataset.sourceDocument == "NOT_PROVIDED" ||
            dataset.measurementMethod == "NOT_PROVIDED" ||
            dataset.waveform == "NOT_PROVIDED" ||
            dataset.temperatureStatus == "NOT_PROVIDED")
            throw std::invalid_argument(
                "Core-loss reference dataset is missing required quality or measurement provenance.");
        return Result<CoreLossReferenceDataset>::success(std::move(dataset));
    } catch (const std::exception& exception) {
        return Result<CoreLossReferenceDataset>::failure(loaderError(
            ErrorCode::InvalidInput,
            std::string("Cannot decode core-loss reference dataset: ") + exception.what(),
            "payload"));
    }
}

std::optional<CoreLossReferencePoint> findExactCoreLossReferencePoint(
    const CoreLossReferenceDataset& dataset,
    units::Frequency frequency,
    units::FluxDensity peakFluxDensity)
{
    for (const auto& point : dataset.points) {
        if (point.frequency.si() == frequency.si() &&
            point.peakFluxDensity.si() == peakFluxDensity.si())
            return point;
    }
    return std::nullopt;
}

} // namespace p135::infrastructure::sqlite
