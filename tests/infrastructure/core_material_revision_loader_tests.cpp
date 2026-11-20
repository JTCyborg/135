#include "p135/infrastructure/sqlite/core_material_revision_loader.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include <cassert>
#include <filesystem>
#include <string>

using namespace p135;
using namespace p135::infrastructure::sqlite;

int main(int argc, char** argv)
{
    assert(argc == 3);
    const auto databasePath =
        (std::filesystem::temp_directory_path() / "project135_core_material_loader_test.sqlite").string();
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
    const auto seeded = seedStarterMaterialCatalog(store, "loader-test", "2026-11-20T00:00:00Z");
    assert(seeded);
    assert(seeded.value() > 0);

    const auto n87 = loadCoreMaterialRevision(store, "SEED-TDK-N87-R1");
    assert(n87);
    assert(n87.value().material.id == "MAT-TDK-N87");
    assert(n87.value().sourceMaterialId == "TDK-N87");
    assert(n87.value().material.grade == "N87");
    assert(n87.value().material.family == domain::CoreMaterialFamily::Ferrite);
    assert(n87.value().material.bhCurve.size() == 8);
    assert(n87.value().material.lossData.size() == 4);
    assert(n87.value().bhCurveStatus == "APPROXIMATE_DIGITIZATION");
    assert(n87.value().designReleaseStatus == "NOT_VERIFIED");
    assert(n87.value().bhCurveFrequency.si() == 10000.0);
    assert(n87.value().bhCurveTemperature.si() == 298.15);
    assert(n87.value().material.lossData.front().specificLoss.si() > 0.0);
    assert(n87.value().material.lossData.front().specificLoss.si() == 57000.0 / 4850.0);

    const auto n97 = loadCoreMaterialRevision(store, "SEED-TDK-N97-R1");
    assert(n97);
    assert(n97.value().material.id == "MAT-TDK-N97");
    assert(n97.value().material.bhCurve.size() == 8);
    assert(n97.value().material.lossData.size() == 4);

    // Published steel seed records do not contain a continuous B-H curve.
    // They must not be silently converted into solver-ready core materials.
    const auto incompleteSteel =
        loadCoreMaterialRevision(store, "SEED-MAT-TK-POWERCORE-M235-35A-R1");
    assert(!incompleteSteel);
    assert(incompleteSteel.error().code == ErrorCode::InvalidInput);

    const auto missing = loadCoreMaterialRevision(store, "DOES-NOT-EXIST");
    assert(!missing);
    assert(missing.error().code == ErrorCode::MissingRequiredData);

    const auto wrongType = loadCoreMaterialRevision(store, "SEED-DUPONT-KAPTON-HN25-R1");
    assert(!wrongType);
    assert(wrongType.error().code == ErrorCode::InvalidInput);

    database.close();
    std::filesystem::remove(databasePath);
    return 0;
}
