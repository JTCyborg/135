#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/revision_bound_winding_electrothermal.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include <cassert>
#include <cmath>
#include <complex>
#include <filesystem>

using namespace p135;
using namespace p135::infrastructure::sqlite;

int main(int argc, char** argv)
{
    assert(argc == 3);
    const auto databasePath =
        (std::filesystem::temp_directory_path() /
         "project135_revision_bound_winding_electrothermal_test.sqlite").string();
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
        store, "revision-bound-electrothermal-test", "2026-11-20T00:00:00Z");
    assert(seeded && seeded.value() > 0);

    thermal::WindingElectrothermalRequest request;
    auto& electrical = request.electrical;
    electrical.material.id = "UNTRUSTED-CALLER-MATERIAL";
    electrical.turns = 100;
    electrical.parallelConductors = 2;
    electrical.meanTurnLength = units::Length{0.5};
    electrical.bareConductorCrossSection = units::Area{1.0e-6};
    electrical.windingTemperature = units::TemperatureKelvin{293.15};
    domain::WindingHarmonicCurrent fundamental;
    fundamental.order = 1;
    fundamental.frequency = units::Frequency{50.0};
    fundamental.phaseRmsCurrent = {
        std::complex<double>{10.0, 0.0},
        std::complex<double>{-5.0, 8.660254037844386},
        std::complex<double>{-5.0, -8.660254037844386}
    };
    electrical.harmonicCurrents.push_back(fundamental);
    request.ambientTemperature = units::TemperatureKelvin{293.15};
    request.thermalResistanceKPerW = 0.5;
    request.maximumWindingTemperature = units::TemperatureKelvin{393.15};
    request.thermalResistanceSourceReference = "measured-fixture:R1";
    request.temperatureLimitSourceReference = "datasheet-fixture:R1";
    request.temperatureToleranceK = 1.0e-7;
    request.relaxationFactor = 0.5;
    request.maximumIterations = 200;

    const auto result = analyzeRevisionBoundWindingElectrothermal(
        store, "SEED-CU-C11000-R1", request);
    assert(result);
    assert(result.value().conductorMaterialRevision.revisionId == "SEED-CU-C11000-R1");
    assert(result.value().electrothermal.converged);
    assert(result.value().electrothermal.status == thermal::AssessmentStatus::NotVerified);
    assert(result.value().electrothermal.explanation.find("not design-released") != std::string::npos);
    assert(result.value().electrothermal.copperLoss.resistanceAtOperatingTemperature.si() > 0.0);

    const auto missing = analyzeRevisionBoundWindingElectrothermal(
        store, "NO-SUCH-CONDUCTOR-REVISION", request);
    assert(!missing && missing.error().code == ErrorCode::MissingRequiredData);

    const auto wrongType = analyzeRevisionBoundWindingElectrothermal(
        store, "SEED-TDK-N87-R1", request);
    assert(!wrongType && wrongType.error().code == ErrorCode::InvalidInput);

    database.close();
    std::filesystem::remove(databasePath);
    return 0;
}
