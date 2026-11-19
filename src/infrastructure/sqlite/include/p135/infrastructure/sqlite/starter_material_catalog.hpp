#pragma once

#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <cstddef>
#include <string>

namespace p135::infrastructure::sqlite {

/// Idempotently inserts curated manufacturer/reference starter records.
/// Existing material identities are never overwritten.
[[nodiscard]] Result<std::size_t> seedStarterMaterialCatalog(
    SqliteMaterialRevisionStore& store,
    const std::string& actor,
    const std::string& timestampUtc);

} // namespace p135::infrastructure::sqlite
