#include "p135/application/material_revision_use_cases.hpp"

#include <cassert>
#include <unordered_map>

using namespace p135;
using namespace p135::application;

namespace {
class FakeRepository final : public MaterialRevisionRepository {
public:
    Result<std::optional<MaterialRevision>> findRevision(const std::string& id) const override
    {
        const auto it = revisions.find(id);
        return Result<std::optional<MaterialRevision>>::success(
            it == revisions.end() ? std::nullopt : std::optional<MaterialRevision>{it->second});
    }
    Result<std::optional<MaterialRevision>> latestRevision(const std::string& id) const override
    {
        const MaterialRevision* latest = nullptr;
        for (const auto& [key, revision] : revisions) {
            (void)key;
            if (revision.materialId == id && (!latest || revision.revisionNumber > latest->revisionNumber))
                latest = &revision;
        }
        return Result<std::optional<MaterialRevision>>::success(
            latest ? std::optional<MaterialRevision>{*latest} : std::nullopt);
    }
    Result<void> appendRevision(const MaterialRevision& revision) override
    {
        if (revisions.contains(revision.revisionId))
            return Result<void>::failure(Error{ErrorCode::InvalidInput, "duplicate revision", {}});
        for (const auto& [key, existing] : revisions) {
            (void)key;
            if (existing.materialId == revision.materialId &&
                existing.revisionNumber == revision.revisionNumber)
                return Result<void>::failure(Error{ErrorCode::InvalidInput, "duplicate revision number", {}});
        }
        revisions.emplace(revision.revisionId, revision);
        return Result<void>::success();
    }
    Result<void> setArchived(const std::string& id, bool value,
                             const std::string&, const std::string&) override
    {
        archived[id] = value;
        return Result<void>::success();
    }
    std::unordered_map<std::string, MaterialRevision> revisions;
    std::unordered_map<std::string, bool> archived;
};
class FakeUsage final : public MaterialUsageQuery {
public:
    Result<bool> isMaterialInUse(const std::string&) const override
    { return Result<bool>::success(inUse); }
    bool inUse{false};
};
MaterialRevision firstRevision()
{
    return {"REV-1", "MAT-1", 1, std::nullopt, "core-material",
            R"({"id":"MAT-1","grade":"fixture"})", "sha256-fixture",
            "test fixture", "fixture-1", "unit-test", "2026-01-01T00:00:00Z"};
}
}

int main()
{
    FakeRepository repository;
    CreateMaterialRevision create(repository);
    auto first = firstRevision();
    assert(create.execute(first));
    assert(!create.execute(first));

    auto second = first;
    second.revisionId = "REV-2";
    second.revisionNumber = 2;
    second.parentRevisionId = "REV-1";
    second.createdAtUtc = "2026-01-02T00:00:00Z";
    assert(create.execute(second));

    auto skipped = second;
    skipped.revisionId = "REV-4";
    skipped.revisionNumber = 4;
    skipped.parentRevisionId = "REV-2";
    assert(!create.execute(skipped));

    auto wrongParent = second;
    wrongParent.revisionId = "REV-X";
    wrongParent.revisionNumber = 3;
    wrongParent.parentRevisionId = "OTHER-MATERIAL-REV";
    assert(!create.execute(wrongParent));

    FakeUsage usage;
    ArchiveMaterial archive(repository, usage);
    usage.inUse = true;
    assert(!archive.execute("MAT-1", "tester", "2026-01-03T00:00:00Z"));
    usage.inUse = false;
    assert(archive.execute("MAT-1", "tester", "2026-01-03T00:00:00Z"));
    assert(repository.archived.at("MAT-1"));
    assert(!archive.execute("MISSING", "tester", "2026-01-03T00:00:00Z"));
    return 0;
}
