#include "p135/infrastructure/sqlite/migration_runner.hpp"

#include <cassert>
#include <filesystem>
#include <string>

using namespace p135::infrastructure::sqlite;

int main(int argc, char** argv)
{
    assert(argc == 2);
    const auto databasePath = std::filesystem::temp_directory_path() /
        "project135_sqlite_migration_tests.db";
    std::error_code ignored;
    std::filesystem::remove(databasePath, ignored);

    {
        SqliteDatabase database;
        const auto opened = database.open(databasePath.string());
        assert(opened);
        assert(database.execute(
            "CREATE TABLE schema_migrations ("
            "version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
            "checksum_sha256 TEXT NOT NULL, applied_at_utc TEXT NOT NULL);"));
        MigrationRunner runner(database);
        const Migration migration{1, "initial_material_revision_store", argv[1]};
        const auto applied = runner.apply(migration);
        assert(applied);
        const auto repeated = runner.apply(migration);
        assert(repeated);
        assert(database.execute(
            "INSERT INTO materials(material_id,record_type,created_at_utc,modified_at_utc) "
            "VALUES('MAT-TEST','core-material','2026-01-01T00:00:00Z','2026-01-01T00:00:00Z');"));
        assert(database.execute(
            "INSERT INTO material_revisions(revision_id,material_id,revision_number,"
            "record_type,payload_json,content_hash,source_reference,dataset_revision,author,created_at_utc) "
            "VALUES('REV-1','MAT-TEST',1,'core-material','{}',"
            "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',"
            "'test','test-1','test','2026-01-01T00:00:00Z');"));
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
