#include "p135/application/material_revision_use_cases.hpp"

#include <utility>

namespace p135::application {
namespace {
Error failure(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "MATERIAL_REVISION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}
}

Result<void> CreateMaterialRevision::execute(const MaterialRevision& revision)
{
    if (revision.revisionId.empty() || revision.materialId.empty() ||
        revision.recordType.empty() || revision.payload.empty())
        return Result<void>::failure(failure(
            ErrorCode::MissingRequiredData,
            "Revision ID, material ID, record type and snapshot payload are required.",
            "revision"));
    if (revision.revisionNumber == 0 || revision.contentHash.empty() ||
        revision.sourceReference.empty() || revision.datasetRevision.empty() ||
        revision.author.empty() || revision.createdAtUtc.empty())
        return Result<void>::failure(failure(
            ErrorCode::MissingRequiredData,
            "Revision number, content hash, provenance, author and UTC timestamp are required.",
            "revisionMetadata"));

    const auto latest = repository_.latestRevision(revision.materialId);
    if (!latest) return Result<void>::failure(latest.error());
    if (!latest.value()) {
        if (revision.revisionNumber != 1 || revision.parentRevisionId)
            return Result<void>::failure(failure(
                ErrorCode::InvalidInput,
                "The first material revision must be revision 1 and have no parent.",
                "parentRevisionId"));
    } else {
        const auto& parent = *latest.value();
        if (!revision.parentRevisionId ||
            *revision.parentRevisionId != parent.revisionId ||
            revision.revisionNumber != parent.revisionNumber + 1)
            return Result<void>::failure(failure(
                ErrorCode::InvalidInput,
                "A new revision must directly extend the latest revision with the next number.",
                "parentRevisionId"));
    }

    if (revision.parentRevisionId) {
        const auto parent = repository_.findRevision(*revision.parentRevisionId);
        if (!parent) return Result<void>::failure(parent.error());
        if (!parent.value() || parent.value()->materialId != revision.materialId)
            return Result<void>::failure(failure(
                ErrorCode::InvalidInput,
                "Parent revision does not belong to this material.",
                "parentRevisionId"));
    }
    return repository_.appendRevision(revision);
}

Result<void> ArchiveMaterial::execute(const std::string& materialId,
                                       const std::string& actor,
                                       const std::string& timestampUtc)
{
    if (materialId.empty() || actor.empty() || timestampUtc.empty())
        return Result<void>::failure(failure(
            ErrorCode::MissingRequiredData,
            "Material ID, actor and UTC timestamp are required to archive a record.",
            "archive"));
    const auto latest = repository_.latestRevision(materialId);
    if (!latest) return Result<void>::failure(latest.error());
    if (!latest.value())
        return Result<void>::failure(failure(
            ErrorCode::MissingRequiredData, "Material does not exist.", "materialId"));

    const auto inUse = usage_.isMaterialInUse(materialId);
    if (!inUse) return Result<void>::failure(inUse.error());
    if (inUse.value())
        return Result<void>::failure(failure(
            ErrorCode::InvalidInput,
            "Material is referenced by a saved product or calculation and cannot be archived.",
            "materialId"));
    return repository_.setArchived(materialId, true, actor, timestampUtc);
}

} // namespace p135::application
