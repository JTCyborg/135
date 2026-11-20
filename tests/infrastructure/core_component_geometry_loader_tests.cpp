#include "p135/infrastructure/sqlite/core_component_geometry_loader.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>

using namespace p135;
using namespace p135::infrastructure::sqlite;

int main(int argc, char** argv)
{
    assert(argc == 3);
    const auto databasePath =
        (std::filesystem::temp_directory_path() / "project135_core_component_geometry_test.sqlite").string();
    std::filesystem::remove(databasePath);

    SqliteDatabase database;
    assert(database.open(databasePath));
    MigrationRunner migrations(database);
    assert(database.execute(
        "CREATE TABLE schema_migrations ("
        "version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
        "checksum_sha256 TEXT NOT NULL, applied_at_utc TEXT NOT NULL);"));
    assert(migrations.apply(Migration{1, "initial_material_revision_store", argv[1]}));
    assert(migrations.apply(Migration{2, "calculation_snapshots", argv[2]}));

    SqliteMaterialRevisionStore store(database);
    const auto seeded = seedStarterMaterialCatalog(store, "geometry-test", "2026-11-20T00:00:00Z");
    assert(seeded);

    const auto pq32 = loadCoreComponentGeometryRevision(store, "SEED-B65879B0000R087-R1");
    assert(pq32);
    assert(pq32.value().orderingCode == "B65879B0000R087");
    assert(pq32.value().coreShape == "PQ 32/30");
    assert(pq32.value().hasMagneticGeometry());
    assert(!pq32.value().hasCompleteCadGeometry());
    assert(std::abs(*pq32.value().effectiveAreaM2 - 153.8e-6) < 1.0e-12);
    assert(std::abs(*pq32.value().minimumAreaM2 - 127.5e-6) < 1.0e-12);
    assert(std::abs(*pq32.value().effectivePathLengthM - 67.8e-3) < 1.0e-12);
    assert(std::abs(*pq32.value().effectiveVolumeM3 - 10440.0e-9) < 1.0e-12);
    assert(std::abs(*pq32.value().coreFactorPerM - 0.441e3) < 1.0e-12);
    assert(pq32.value().nominalAlHPerTurnSquared.has_value());
    assert(std::abs(*pq32.value().nominalAlHPerTurnSquared - 4.8e-6) < 1.0e-12);

    const auto pq40 = loadCoreComponentGeometryRevision(store, "SEED-B65883A0000R087-R1");
    assert(pq40 && pq40.value().hasMagneticGeometry());
    assert(std::abs(*pq40.value().effectiveAreaM2 - 188.3e-6) < 1.0e-12);
    assert(std::abs(*pq40.value().effectivePathLengthM - 92.5e-3) < 1.0e-12);

    const auto geometryUnavailable =
        loadCoreComponentGeometryRevision(store, "SEED-B65877B0000R087-R1");
    assert(geometryUnavailable);
    assert(!geometryUnavailable.value().hasMagneticGeometry());
    assert(geometryUnavailable.value().geometryStatus == "DIMENSIONS_NOT_IMPORTED");

    const auto wrongType =
        loadCoreComponentGeometryRevision(store, "SEED-TDK-N87-R1");
    assert(!wrongType && wrongType.error().code == ErrorCode::InvalidInput);
    const auto missing = loadCoreComponentGeometryRevision(store, "DOES-NOT-EXIST");
    assert(!missing && missing.error().code == ErrorCode::MissingRequiredData);

    database.close();
    std::filesystem::remove(databasePath);
    return 0;
}
