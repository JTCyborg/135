#include "p135/infrastructure/sqlite/canonical_json.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"
#include "p135/application/material_revision_use_cases.hpp"

#include <cassert>
#include <filesystem>
#include <string>

using namespace p135::infrastructure::sqlite;
using p135::application::MaterialRevision;
using p135::application::CompareMaterialRevisions;
using p135::application::RestoreMaterialRevision;

int main(int argc, char** argv)
{
    assert(argc == 2);
    const auto canonical = canonicalizeJson("{ \"b\": 2, \"a\": 1 }");
    assert(canonical && canonical.value() == "{\"a\":1,\"b\":2}");
    assert(!canonicalizeJson("{"));
    const auto knownHash = sha256Hex("{\"a\":1}");
    assert(knownHash &&
        knownHash.value() == "015abd7f5cc57a2dd94b7590f04ad8084273905ee33ec5cebeae62276a97f862");
    const auto databasePath = std::filesystem::temp_directory_path() /
        "project135_sqlite_material_store_tests.db";
    std::error_code ignored;
    std::filesystem::remove(databasePath, ignored);

    {
        SqliteDatabase database;
        assert(database.open(databasePath.string()));
        assert(database.execute(
            "CREATE TABLE schema_migrations ("
            "version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
            "checksum_sha256 TEXT NOT NULL, applied_at_utc TEXT NOT NULL);"));
        MigrationRunner runner(database);
        const Migration migration{1, "initial_material_revision_store", argv[1]};
        assert(runner.apply(migration));
        assert(runner.apply(migration));

        SqliteMaterialRevisionStore store(database);
        MaterialRevision first{
            "REV-1", "MAT-TEST", 1, std::nullopt, "core-material", "{}",
            "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
            "test://datasheet", "dataset-1", "test-author", "2026-01-01T00:00:00Z"
        };
        assert(store.appendRevision(first));
        const auto initiallyArchived = store.isArchived("MAT-TEST");
        assert(initiallyArchived && !initiallyArchived.value());

        const auto found = store.findRevision("REV-1");
        assert(found && found.value());
        assert(found.value()->payload == "{}");
        assert(found.value()->contentHash == first.contentHash);
        const auto latest = store.latestRevision("MAT-TEST");
        assert(latest && latest.value() && latest.value()->revisionNumber == 1);

        auto invalidHash = first;
        invalidHash.revisionId = "REV-BAD";
        invalidHash.contentHash = std::string(64, '0');
        assert(!store.appendRevision(invalidHash));

        auto nonCanonical = first;
        nonCanonical.revisionId = "REV-WHITESPACE";
        nonCanonical.payload = "{ }";
        nonCanonical.contentHash = sha256Hex(nonCanonical.payload).value();
        assert(!store.appendRevision(nonCanonical));

        MaterialRevision second{
            "REV-2", "MAT-TEST", 2, std::string("REV-1"), "core-material", "{\"a\":1}",
            "015abd7f5cc57a2dd94b7590f04ad8084273905ee33ec5cebeae62276a97f862",
            "test://datasheet", "dataset-2", "test-author", "2026-01-02T00:00:00Z"
        };
        assert(store.appendRevision(second));
        const auto latestSecond = store.latestRevision("MAT-TEST");
        assert(latestSecond && latestSecond.value() && latestSecond.value()->revisionNumber == 2);
        const auto history = store.revisionHistory("MAT-TEST");
        assert(history && history.value().size() == 2);
        assert(history.value()[0].revisionNumber == 1 && history.value()[1].revisionNumber == 2);
        MaterialCatalogFilter catalogFilter;
        catalogFilter.searchText = "mat-test";
        const auto catalog = store.searchLatestRevisions(catalogFilter);
        assert(catalog && catalog.value().size() == 1);
        assert(catalog.value()[0].revisionId == "REV-2");
        catalogFilter.pageOffset = 1;
        const auto nextPage = store.searchLatestRevisions(catalogFilter);
        assert(nextPage && nextPage.value().empty());
        catalogFilter.pageSize = 0;
        assert(!store.searchLatestRevisions(catalogFilter));

        assert(store.setArchived("MAT-TEST", true, "test-author", "2026-01-03T00:00:00Z"));
        const auto archivedState = store.isArchived("MAT-TEST");
        assert(archivedState && archivedState.value());
        MaterialCatalogFilter activeOnly;
        assert(store.searchLatestRevisions(activeOnly).value().empty());
        activeOnly.includeArchived = true;
        assert(store.searchLatestRevisions(activeOnly).value().size() == 1);
        assert(!store.appendRevision(MaterialRevision{
            "REV-3", "MAT-TEST", 3, std::string("REV-2"), "core-material", "{}",
            first.contentHash, "test://datasheet", "dataset-3", "test-author",
            "2026-01-04T00:00:00Z"
        }));
        assert(store.setArchived("MAT-TEST", false, "test-author", "2026-01-05T00:00:00Z"));
        const auto activeState = store.isArchived("MAT-TEST");
        assert(activeState && !activeState.value());

        assert(store.recordUsage(MaterialUsageReference{
            "USE-1", "REV-1", MaterialUsageKind::Product, "PRODUCT-1",
            "2026-01-06T00:00:00Z", "test-author"
        }));
        assert(store.recordUsage(MaterialUsageReference{
            "USE-2", "REV-2", MaterialUsageKind::Calculation, "CALC-1",
            "2026-01-06T00:00:01Z", "test-author"
        }));
        const auto inUse = store.isMaterialInUse("MAT-TEST");
        assert(inUse && inUse.value());
        assert(!store.setArchived("MAT-TEST", true, "test-author", "2026-01-07T00:00:00Z"));
        assert(!database.execute("UPDATE material_revisions SET payload_json='changed';"));
        CompareMaterialRevisions compare(store);
        const auto comparison = compare.execute("REV-1", "REV-2");
        assert(comparison);
        assert(!comparison.value().payloadEqual);
        assert(comparison.value().recordTypeEqual);

        RestoreMaterialRevision restore(store);
        const auto restored = restore.execute(
            "REV-1", "REV-3", "integration-test", "2026-01-08T00:00:00Z");
        assert(restored);
        assert(restored.value().revisionNumber == 3);
        assert(restored.value().parentRevisionId == std::optional<std::string>{"REV-2"});
        assert(restored.value().payload == first.payload);
        assert(restored.value().sourceReference == "restore:REV-1");
        const auto restoredLatest = store.latestRevision("MAT-TEST");
        assert(restoredLatest && restoredLatest.value());
        assert(restoredLatest.value()->revisionId == "REV-3");
        const auto restoredHistory = store.revisionHistory("MAT-TEST");
        assert(restoredHistory && restoredHistory.value().size() == 3);
        const auto seeded = seedStarterMaterialCatalog(store, "integration-test", "2026-11-19T00:00:00Z");
        assert(seeded && seeded.value() == 4);
        const auto seededAgain = seedStarterMaterialCatalog(store, "integration-test", "2026-11-19T00:00:01Z");
        assert(seededAgain && seededAgain.value() == 0);
        MaterialCatalogFilter coreMaterials;
        coreMaterials.recordType = "core-material";
        const auto coreCatalog = store.searchLatestRevisions(coreMaterials);
        assert(coreCatalog && coreCatalog.value().size() == 2);
        const auto n87 = store.latestRevision("MAT-TDK-N87");
        assert(n87 && n87.value());
        assert(n87.value()->payload.find("APPROXIMATE_DIGITIZATION") != std::string::npos);
        const auto copper = store.latestRevision("MAT-CU-C11000-ETP");
        assert(copper && copper.value());
        assert(copper.value()->payload.find("1.707e-08") != std::string::npos);

    }

    {
        SqliteDatabase reopened;
        assert(reopened.open(databasePath.string()));
        assert(reopened.execute("SELECT 1;"));
    }

    std::filesystem::remove(databasePath, ignored);
    return 0;
}
