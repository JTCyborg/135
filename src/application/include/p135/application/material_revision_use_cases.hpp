#pragma once

#include "p135/core/result.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace p135::application {

/// Immutable serialized snapshot of one material or component revision.
/// payload is canonical UTF-8 JSON produced by the infrastructure codec.
struct MaterialRevision {
    std::string revisionId;
    std::string materialId;
    std::uint64_t revisionNumber{};
    std::optional<std::string> parentRevisionId;
    std::string recordType;
    std::string payload;
    std::string contentHash; // SHA-256 hex of canonical payload
    std::string sourceReference;
    std::string datasetRevision;
    std::string author;
    std::string createdAtUtc;
};

struct MaterialRevisionRepository {
    virtual ~MaterialRevisionRepository() = default;
    [[nodiscard]] virtual Result<std::optional<MaterialRevision>>
    findRevision(const std::string& revisionId) const = 0;
    [[nodiscard]] virtual Result<std::optional<MaterialRevision>>
    latestRevision(const std::string& materialId) const = 0;
    /// Must atomically enforce unique revision IDs/numbers and immutable inserts.
    [[nodiscard]] virtual Result<void> appendRevision(const MaterialRevision& revision) = 0;
    /// Archive state is separate from revision payload and must be audited by the repository.
    [[nodiscard]] virtual Result<void> setArchived(const std::string& materialId,
                                                   bool archived,
                                                   const std::string& actor,
                                                   const std::string& timestampUtc) = 0;
};

struct MaterialUsageQuery {
    virtual ~MaterialUsageQuery() = default;
    [[nodiscard]] virtual Result<bool> isMaterialInUse(const std::string& materialId) const = 0;
};

class CreateMaterialRevision {
public:
    explicit CreateMaterialRevision(MaterialRevisionRepository& repository)
        : repository_(repository) {}
    [[nodiscard]] Result<void> execute(const MaterialRevision& revision);
private:
    MaterialRevisionRepository& repository_;
};

class ArchiveMaterial {
public:
    ArchiveMaterial(MaterialRevisionRepository& repository, const MaterialUsageQuery& usage)
        : repository_(repository), usage_(usage) {}
    [[nodiscard]] Result<void> execute(const std::string& materialId,
                                       const std::string& actor,
                                       const std::string& timestampUtc);
private:
    MaterialRevisionRepository& repository_;
    const MaterialUsageQuery& usage_;
};

} // namespace p135::application
