#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <cassert>
#include <filesystem>
#include <string>

using namespace p135::infrastructure::sqlite;
using p135::application::MaterialRevision;

int main(int argc, char** argv)
{
    assert(argc == 2);
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

        MaterialRevision second{
            "REV-2", "MAT-TEST", 2, std::string("REV-1"), "core-material", "{ }",
            "cdb4ee2aea69cc6a8b8c4d0f8a6e3f6b4c7b1c5e8d9f0a1b2c3d4e5f60718293",
            "test://datasheet", "dataset-2", "test-author", "2026-01-02T00:00:00Z"
        };
        // Use a payload whose SHA-256 is known through OpenSSL in the adapter contract:
        // malformed content hashes are rejected before any database mutation.
        assert(!store.appendRevision(second));

        assert(database.execute(
            "INSERT INTO material_usage(usage_id,revision_id,product_id,usage_kind,created_at_utc) "
            "VALUES('USE-1','REV-1','PRODUCT-1','product','2026-01-03T00:00:00Z');"));
        const auto inUse = store.isMaterialInUse("MAT-TEST");
        assert(inUse && inUse.value());

        assert(store.setArchived("MAT-TEST", true, "test-author", "2026-01-04T00:00:00Z"));
        assert(!store.appendRevision(MaterialRevision{
            "REV-3", "MAT-TEST", 2, std::string("REV-1"), "core-material", "{}",
            first.contentHash, "test://datasheet", "dataset-3", "test-author",
            "2026-01-05T00:00:00Z"
        }));
        assert(!database.execute("UPDATE material_revisions SET payload_json='changed';"));
    }

    {
        SqliteDatabase reopened;
        assert(reopened.open(databasePath.string()));
        assert(reopened.execute("SELECT 1;"));
    }

    std::filesystem::remove(databasePath, ignored);
    return 0;
}
