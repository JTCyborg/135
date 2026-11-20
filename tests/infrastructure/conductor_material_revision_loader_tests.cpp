#include "p135/infrastructure/sqlite/conductor_material_revision_loader.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <string>

using namespace p135;
using namespace p135::infrastructure::sqlite;

int main(int argc, char** argv)
{
    assert(argc == 3);
    const auto databasePath =
        (std::filesystem::temp_directory_path() /
         "project135_conductor_material_loader_test.sqlite").string();
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
    const auto seeded = seedStarterMaterialCatalog(
        store, "conductor-loader-test", "2026-11-20T00:00:00Z");
    assert(seeded && seeded.value() > 0);

    const auto copper = loadConductorMaterialRevision(store, "SEED-CU-C11000-R1");
    assert(copper);
    assert(copper.value().material.id == "MAT-CU-C11000-ETP");
    assert(copper.value().sourceMaterialId == "CU-C11000-ETP");
    assert(copper.value().material.grade ==
           "C11000 ETP copper, annealed conductivity reference");
    assert(std::abs(copper.value().material.resistivityAt20C.si() - 1.707e-8) < 1.0e-20);
    assert(copper.value().material.temperatureCoefficient.si() == 0.00393);
    assert(copper.value().material.density.si() == 8910.0);
    assert(copper.value().resistivityStatus == "DERIVED_FROM_101_PERCENT_IACS");
    assert(copper.value().designReleaseStatus == "NOT_VERIFIED");

    const auto enamelled =
        loadConductorMaterialRevision(store, "SEED-ELEKTRISOLA-P180-R1");
    assert(enamelled);
    assert(enamelled.value().material.id == "MAT-ELEKTRISOLA-POLYSOL-180");
    assert(enamelled.value().material.resistivityAt20C.si() == 1.71e-8);
    assert(enamelled.value().material.temperatureCoefficient.si() == 0.00393);
    assert(enamelled.value().designReleaseStatus == "NOT_VERIFIED");
    // Catalog wire-diameter ranges are not interpreted as a selected conductor size.

    const auto wrongType = loadConductorMaterialRevision(store, "SEED-TDK-N87-R1");
    assert(!wrongType);
    assert(wrongType.error().code == ErrorCode::InvalidInput);

    const auto missing = loadConductorMaterialRevision(store, "NO-SUCH-CONDUCTOR-REVISION");
    assert(!missing);
    assert(missing.error().code == ErrorCode::MissingRequiredData);

    database.close();
    std::filesystem::remove(databasePath);
    return 0;
}
