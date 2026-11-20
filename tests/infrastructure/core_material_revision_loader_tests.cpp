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
    assert(n87.value().bhCurveSourceDocument == "TDK FAL0599-4");
    assert(n87.value().bhCurveSourcePage == "NOT_STATED_IN_IMPORTED_REFERENCE");
    assert(n87.value().bhCurveMeasurementMethod == "NOT_STATED_IN_IMPORTED_REFERENCE");
    assert(n87.value().bhCurveSampleDirection == "NOT_STATED_IN_IMPORTED_REFERENCE");
    assert(n87.value().bhCurveCaptureMethod == "APPROXIMATE_DIGITIZATION_FROM_MANUFACTURER_GRAPH");
    assert(n87.value().bhCurveNotes.find("raw tabular B-H") != std::string::npos);
    assert(n87.value().designReleaseStatus == "NOT_VERIFIED");
    assert(n87.value().bhCurveFrequency.si() == 10000.0);
    assert(n87.value().bhCurveTemperature.has_value());
    assert(n87.value().bhCurveTemperature->si() == 298.15);
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
        loadCoreMaterialRevision(store, "SEED-TK-M235-35A-R1");
    assert(!incompleteSteel);
    assert(incompleteSteel.error().code == ErrorCode::InvalidInput);

    // The Kobe Steel table contains measured DC initial-magnetization data,
    // not an AC B-H loop or core-loss model. Preserve exactly that distinction.
    const auto steel35A300 =
        loadCoreMaterialRevision(store, "SEED-MAT-RESEARCH-35A300-DC-R1");
    assert(steel35A300);
    assert(steel35A300.value().material.dataSource == domain::MaterialDataSource::Measurement);
    assert(steel35A300.value().material.grade == "35A300 non-oriented electrical steel");
    assert(steel35A300.value().bhCurveStatus == "MEASURED_TABULAR");
    assert(steel35A300.value().material.bhCurve.size() == 8);
    assert(steel35A300.value().material.bhCurve.front().fieldStrength.si() == 100.0);
    assert(steel35A300.value().material.bhCurve.front().fluxDensity.si() == 1.20);
    assert(steel35A300.value().material.bhCurve.back().fieldStrength.si() == 20000.0);
    assert(steel35A300.value().material.bhCurve.back().fluxDensity.si() == 1.93);
    assert(steel35A300.value().bhCurveFrequency.si() == 0.0);
    assert(!steel35A300.value().bhCurveTemperature.has_value());
    assert(steel35A300.value().material.lossData.empty());
    assert(steel35A300.value().coreLossStatus == "NOT_PUBLISHED");
    assert(steel35A300.value().bhCurveSourceDocument ==
           "Kobe Steel Engineering Reports, Vol. 71 No. 2, July 2022");
    assert(steel35A300.value().bhCurveSourcePage == "19, Table 2");
    assert(steel35A300.value().bhCurveSampleDirection == "NOT_STATED_IN_PUBLICATION");
    assert(steel35A300.value().bhCurveCaptureMethod ==
           "MEASURED_TABULAR_VALUES_TRANSCRIBED_FROM_TABLE_2");
    assert(steel35A300.value().designReleaseStatus == "NOT_VERIFIED");

    const auto steel50A800 =
        loadCoreMaterialRevision(store, "SEED-MAT-RESEARCH-50A800-DC-R1");
    assert(steel50A800);
    assert(steel50A800.value().material.bhCurve.size() == 8);
    assert(steel50A800.value().material.bhCurve.front().fluxDensity.si() == 0.22);
    assert(steel50A800.value().material.bhCurve.back().fluxDensity.si() == 1.97);
    assert(steel50A800.value().material.lossData.empty());
    assert(steel50A800.value().coreLossStatus == "NOT_PUBLISHED");

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
