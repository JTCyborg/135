#include "p135/infrastructure/sqlite/core_component_geometry_loader.hpp"

#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace p135::infrastructure::sqlite {
namespace {
using nlohmann::json;

Error componentError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CORE_COMPONENT_GEOMETRY_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

std::string requiredString(const json& object, const char* key)
{
    const auto& value = object.at(key);
    if (!value.is_string() || value.get<std::string>().empty())
        throw std::invalid_argument(std::string("Missing or invalid string field: ") + key);
    return value.get<std::string>();
}

std::optional<double> optionalPositive(const json& object, const char* key)
{
    const auto found = object.find(key);
    if (found == object.end() || found->is_null())
        return std::nullopt;
    if (!found->is_number())
        throw std::invalid_argument(std::string("Expected numeric geometry field: ") + key);
    const double value = found->get<double>();
    if (!std::isfinite(value) || value <= 0.0)
        throw std::invalid_argument(std::string("Geometry field must be finite and positive: ") + key);
    return value;
}
}

bool CoreComponentGeometrySnapshot::hasMagneticGeometry() const noexcept
{
    return effectiveAreaM2 && minimumAreaM2 && effectivePathLengthM &&
        effectiveVolumeM3 && coreFactorPerM &&
        *effectiveAreaM2 > 0.0 && *minimumAreaM2 > 0.0 &&
        *effectivePathLengthM > 0.0 && *effectiveVolumeM3 > 0.0 &&
        *coreFactorPerM > 0.0;
}

bool CoreComponentGeometrySnapshot::hasCompleteCadGeometry() const noexcept
{
    return cadGeometryStatus == "COMPLETE_PARAMETRIC_PROFILE";
}

Result<CoreComponentGeometrySnapshot> loadCoreComponentGeometryRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& revisionId)
{
    if (revisionId.empty())
        return Result<CoreComponentGeometrySnapshot>::failure(componentError(
            ErrorCode::MissingRequiredData, "An exact core-component revision ID is required.",
            "revisionId"));

    const auto stored = store.findRevision(revisionId);
    if (!stored)
        return Result<CoreComponentGeometrySnapshot>::failure(stored.error());
    if (!stored.value())
        return Result<CoreComponentGeometrySnapshot>::failure(componentError(
            ErrorCode::MissingRequiredData, "The requested core-component revision does not exist.",
            "revisionId"));

    const auto& revision = *stored.value();
    if (revision.recordType != "core-component")
        return Result<CoreComponentGeometrySnapshot>::failure(componentError(
            ErrorCode::InvalidInput, "The requested revision is not a core-component record.",
            "recordType"));

    const auto canonical = canonicalizeJson(revision.payload);
    if (!canonical)
        return Result<CoreComponentGeometrySnapshot>::failure(canonical.error());
    if (canonical.value() != revision.payload)
        return Result<CoreComponentGeometrySnapshot>::failure(componentError(
            ErrorCode::InvalidInput, "Stored component payload is not canonical JSON.", "payload"));
    const auto hash = sha256Hex(revision.payload);
    if (!hash)
        return Result<CoreComponentGeometrySnapshot>::failure(hash.error());
    if (hash.value() != revision.contentHash)
        return Result<CoreComponentGeometrySnapshot>::failure(componentError(
            ErrorCode::InvalidInput, "Stored component payload hash does not match its revision.",
            "contentHash"));

    try {
        const auto root = json::parse(revision.payload);
        if (!root.is_object() || !root.contains("component") ||
            !root.at("component").is_object())
            throw std::invalid_argument("Component object is missing.");
        const auto& component = root.at("component");
        CoreComponentGeometrySnapshot snapshot;
        snapshot.revisionId = revision.revisionId;
        snapshot.componentId = requiredString(component, "id");
        snapshot.orderingCode = requiredString(component, "orderingCode");
        snapshot.manufacturer = requiredString(component, "manufacturer");
        snapshot.coreShape = requiredString(component, "coreShape");
        snapshot.ferriteMaterial = requiredString(component, "ferriteMaterial");
        snapshot.sourceReference = revision.sourceReference;
        snapshot.datasetRevision = revision.datasetRevision;

        const auto qualityIt = root.find("dataQuality");
        if (qualityIt == root.end() || !qualityIt->is_object())
            throw std::invalid_argument("Component dataQuality object is missing.");
        const auto& quality = *qualityIt;
        snapshot.geometryStatus = requiredString(quality, "geometryStatus");
        const auto cadStatus = quality.find("cadGeometryStatus");
        if (cadStatus != quality.end() && cadStatus->is_string())
            snapshot.cadGeometryStatus = cadStatus->get<std::string>();

        const auto geometryIt = root.find("magneticGeometry");
        if (geometryIt != root.end() && !geometryIt->is_null()) {
            if (!geometryIt->is_object())
                throw std::invalid_argument("magneticGeometry must be an object.");
            const auto& geometry = *geometryIt;
            snapshot.effectiveAreaM2 = optionalPositive(geometry, "effectiveArea_m2");
            snapshot.minimumAreaM2 = optionalPositive(geometry, "minimumArea_m2");
            snapshot.effectivePathLengthM = optionalPositive(geometry, "effectivePathLength_m");
            snapshot.effectiveVolumeM3 = optionalPositive(geometry, "effectiveVolume_m3");
            snapshot.coreFactorPerM = optionalPositive(geometry, "coreFactor_per_m");
        }
        snapshot.nominalAlHPerTurnSquared =
            optionalPositive(component, "inductanceFactor_AL_H_per_turn_squared");

        if (snapshot.hasMagneticGeometry()) {
            const double areaFromVolume = *snapshot.effectiveVolumeM3 /
                *snapshot.effectivePathLengthM;
            if (std::abs(areaFromVolume - *snapshot.effectiveAreaM2) >
                *snapshot.effectiveAreaM2 * 0.03)
                throw std::invalid_argument(
                    "Ae, le and Ve are inconsistent beyond the 3% datasheet-rounding tolerance.");
            const double factorFromGeometry = *snapshot.coreFactorPerM /
                *snapshot.effectiveAreaM2;
            if (!std::isfinite(factorFromGeometry) || factorFromGeometry <= 0.0)
                throw std::invalid_argument("Core factor and effective area are inconsistent.");
        }
        return Result<CoreComponentGeometrySnapshot>::success(std::move(snapshot));
    } catch (const std::exception& exception) {
        return Result<CoreComponentGeometrySnapshot>::failure(componentError(
            ErrorCode::InvalidInput,
            std::string("Cannot decode core-component geometry revision: ") + exception.what(),
            "payload"));
    }
}

} // namespace p135::infrastructure::sqlite
