#include "p135/application/material_revision_use_cases.hpp"

#include <utility>
#include <limits>

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

Result<MaterialRevisionComparison> CompareMaterialRevisions::execute(
    const std::string& leftRevisionId, const std::string& rightRevisionId) const
{
    if (leftRevisionId.empty() || rightRevisionId.empty())
        return Result<MaterialRevisionComparison>::failure(failure(
            ErrorCode::MissingRequiredData, "Both revision IDs are required for comparison.", "revisionIds"));
    const auto left = repository_.findRevision(leftRevisionId);
    if (!left) return Result<MaterialRevisionComparison>::failure(left.error());
    if (!left.value()) return Result<MaterialRevisionComparison>::failure(failure(
        ErrorCode::MissingRequiredData, "Left revision does not exist.", "leftRevisionId"));
    const auto right = repository_.findRevision(rightRevisionId);
    if (!right) return Result<MaterialRevisionComparison>::failure(right.error());
    if (!right.value()) return Result<MaterialRevisionComparison>::failure(failure(
        ErrorCode::MissingRequiredData, "Right revision does not exist.", "rightRevisionId"));
    const auto& a = *left.value();
    const auto& b = *right.value();
    return Result<MaterialRevisionComparison>::success(MaterialRevisionComparison{
        a, b, a.payload == b.payload, a.recordType == b.recordType,
        a.sourceReference == b.sourceReference && a.datasetRevision == b.datasetRevision});
}

Result<MaterialRevision> RestoreMaterialRevision::execute(
    const std::string& sourceRevisionId, const std::string& newRevisionId,
    const std::string& actor, const std::string& timestampUtc)
{
    if (sourceRevisionId.empty() || newRevisionId.empty() || actor.empty() || timestampUtc.empty())
        return Result<MaterialRevision>::failure(failure(
            ErrorCode::MissingRequiredData,
            "Source revision, new revision ID, actor and UTC timestamp are required.", "restore"));
    const auto source = repository_.findRevision(sourceRevisionId);
    if (!source) return Result<MaterialRevision>::failure(source.error());
    if (!source.value()) return Result<MaterialRevision>::failure(failure(
        ErrorCode::MissingRequiredData, "Source revision does not exist.", "sourceRevisionId"));
    const auto latest = repository_.latestRevision(source.value()->materialId);
    if (!latest) return Result<MaterialRevision>::failure(latest.error());
    if (!latest.value()) return Result<MaterialRevision>::failure(failure(
        ErrorCode::MissingRequiredData, "Material has no current revision.", "materialId"));
    if (latest.value()->revisionNumber == std::numeric_limits<std::uint64_t>::max())
        return Result<MaterialRevision>::failure(failure(
            ErrorCode::InvalidInput, "Material revision number is exhausted.", "revisionNumber"));
    MaterialRevision restored = *source.value();
    restored.revisionId = newRevisionId;
    restored.revisionNumber = latest.value()->revisionNumber + 1;
    restored.parentRevisionId = latest.value()->revisionId;
    restored.sourceReference = "restore:" + sourceRevisionId;
    restored.author = actor;
    restored.createdAtUtc = timestampUtc;
    CreateMaterialRevision create(repository_);
    const auto created = create.execute(restored);
    if (!created) return Result<MaterialRevision>::failure(created.error());
    return Result<MaterialRevision>::success(std::move(restored));
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
