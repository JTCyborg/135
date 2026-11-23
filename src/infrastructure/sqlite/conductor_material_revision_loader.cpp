#include "p135/infrastructure/sqlite/conductor_material_revision_loader.hpp"

#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace p135::infrastructure::sqlite {
namespace {

using nlohmann::json;

Error conductorLoaderError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CONDUCTOR_MATERIAL_REVISION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
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
    auto result = value.get<std::string>();
    if (result.empty())
        throw std::invalid_argument(std::string("Empty string field: ") + key);
    return result;
}

} // namespace

Result<ConductorMaterialRevisionSnapshot> loadConductorMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId)
{
    if (revisionId.empty())
        return Result<ConductorMaterialRevisionSnapshot>::failure(conductorLoaderError(
            ErrorCode::MissingRequiredData, "An exact conductor material revision ID is required.",
            "revisionId"));

    const auto stored = store.findRevision(revisionId);
    if (!stored)
        return Result<ConductorMaterialRevisionSnapshot>::failure(stored.error());
    if (!stored.value())
        return Result<ConductorMaterialRevisionSnapshot>::failure(conductorLoaderError(
            ErrorCode::MissingRequiredData, "The requested conductor material revision does not exist.",
            "revisionId"));

    const auto& revision = *stored.value();
    if (revision.recordType != "conductor-material")
        return Result<ConductorMaterialRevisionSnapshot>::failure(conductorLoaderError(
            ErrorCode::InvalidInput, "The requested revision is not a conductor-material record.",
            "recordType"));

    const auto canonical = canonicalizeJson(revision.payload);
    if (!canonical)
        return Result<ConductorMaterialRevisionSnapshot>::failure(canonical.error());
    if (canonical.value() != revision.payload)
        return Result<ConductorMaterialRevisionSnapshot>::failure(conductorLoaderError(
            ErrorCode::InvalidInput, "Stored material payload is not canonical JSON.", "payload"));
    const auto hash = sha256Hex(revision.payload);
    if (!hash)
        return Result<ConductorMaterialRevisionSnapshot>::failure(hash.error());
    if (hash.value() != revision.contentHash)
        return Result<ConductorMaterialRevisionSnapshot>::failure(conductorLoaderError(
            ErrorCode::InvalidInput, "Stored material payload hash does not match its revision.",
            "contentHash"));

    try {
        const auto root = json::parse(revision.payload);
        if (!root.is_object() || requiredNumber(root, "schemaVersion") != 1.0)
            throw std::invalid_argument("Unsupported conductor material payload schema version.");

        const auto& source = root.at("material");
        domain::ConductorMaterial material;
        const auto sourceMaterialId = requiredString(source, "id");
        material.id = revision.materialId;
        material.manufacturer = requiredString(source, "manufacturer");
        material.grade = requiredString(source, "grade");
        material.dataSource = domain::MaterialDataSource::ManufacturerDatasheet;
        material.sourceReference = revision.sourceReference;
        material.datasetRevision = revision.datasetRevision;
        material.resistivityAt20C = units::Resistivity{
            requiredNumber(source, "resistivityAt20C_ohm_m")};
        material.temperatureCoefficient = units::TemperatureCoefficient{
            requiredNumber(source, "temperatureCoefficientAt20C_per_K")};
        material.density = units::Density{requiredNumber(source, "density_kg_per_m3")};

        const auto& quality = root.at("dataQuality");
        ConductorMaterialRevisionSnapshot snapshot;
        snapshot.material = std::move(material);
        snapshot.revisionId = revision.revisionId;
        snapshot.sourceMaterialId = sourceMaterialId;
        snapshot.contentHash = revision.contentHash;
        snapshot.sourceReference = revision.sourceReference;
        snapshot.datasetRevision = revision.datasetRevision;
        // Schema-v1 starter catalog records publish the conductor's
        // electrical-property quality under electricalPropertiesStatus.
        // Older imported records used resistivityStatus; accept that exact
        // legacy field when present, but never synthesize a quality status.
        snapshot.resistivityStatus = quality.contains("electricalPropertiesStatus")
            ? requiredString(quality, "electricalPropertiesStatus")
            : requiredString(quality, "resistivityStatus");
        snapshot.designReleaseStatus = requiredString(quality, "designReleaseStatus");
        snapshot.notes = requiredString(quality, "notes");

        const auto validated = domain::validate(snapshot.material);
        if (!validated)
            return Result<ConductorMaterialRevisionSnapshot>::failure(validated.error());
        return Result<ConductorMaterialRevisionSnapshot>::success(std::move(snapshot));
    } catch (const std::exception& exception) {
        return Result<ConductorMaterialRevisionSnapshot>::failure(conductorLoaderError(
            ErrorCode::InvalidInput,
            std::string("Cannot decode conductor material revision: ") + exception.what(),
            "payload"));
    }
}

} // namespace p135::infrastructure::sqlite
