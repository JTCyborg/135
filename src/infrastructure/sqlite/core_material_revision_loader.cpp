#include "p135/infrastructure/sqlite/core_material_revision_loader.hpp"

#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
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

        const auto& sourceBh = root.at("bhCurve");
        const double bhFrequency = requiredNumber(sourceBh, "frequency_Hz");
        const double bhTemperature = requiredNumber(sourceBh, "temperature_K");
        const auto& bhPoints = sourceBh.at("points");
        if (!bhPoints.is_array() || bhPoints.size() < 2)
            throw std::invalid_argument("At least two source B-H points are required.");
        for (const auto& point : bhPoints) {
            material.bhCurve.push_back(domain::BHPoint{
                units::MagneticFieldStrength{requiredNumber(point, "fieldStrength_A_per_m")},
                units::FluxDensity{requiredNumber(point, "fluxDensity_T")}
            });
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
        const json lossReferencePoints = root.value("coreLossReferencePoints", json::array());
        if (!lossReferencePoints.is_array())
            throw std::invalid_argument("Core-loss reference points must be an array.");
        for (const auto& point : lossReferencePoints) {
            const double temperatureK = point.value("temperature_K", 0.0);
            if (temperatureK < 0.0)
                throw std::invalid_argument("Core-loss reference temperature cannot be negative.");
            snapshot.coreLossReferencePoints.push_back(CoreLossReferencePoint{
                units::Frequency{requiredNumber(point, "frequency_Hz")},
                units::FluxDensity{requiredNumber(point, "peakFluxDensity_T")},
                units::CoreLossDensity{requiredNumber(point, "specificLoss_W_per_kg")},
                temperatureK > 0.0
                    ? std::optional<units::TemperatureKelvin>{units::TemperatureKelvin{temperatureK}}
                    : std::nullopt
            });
        }
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
        const std::string bhCurveType = requiredString(sourceBh, "curveType");
        if (bhCurveType == "DC initial magnetization curve") {
            if (bhFrequency != 0.0)
                throw std::invalid_argument("A DC B-H curve must declare frequency_Hz as zero.");
        } else if (bhFrequency <= 0.0) {
            throw std::invalid_argument("A non-DC B-H curve must declare a positive frequency.");
        }
        snapshot.bhCurveFrequency = units::Frequency{bhFrequency};
        snapshot.bhCurveTemperature = bhTemperature > 0.0
            ? std::optional<units::TemperatureKelvin>{units::TemperatureKelvin{bhTemperature}}
            : std::nullopt;

        const auto validated = domain::validate(snapshot.material);
        if (!validated)
            return Result<CoreMaterialRevisionSnapshot>::failure(validated.error());
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

} // namespace p135::infrastructure::sqlite
