#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/revision_bound_winding_copper_loss.hpp"
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
         "project135_revision_bound_winding_loss_test.sqlite").string();
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
        store, "revision-bound-winding-test", "2026-11-20T00:00:00Z");
    assert(seeded && seeded.value() > 0);

    RevisionBoundWindingCopperLossRequest request;
    request.conductorMaterialRevisionId = "SEED-CU-C11000-R1";
    request.filterBranchId = 17;
    request.turns = 10;
    request.parallelConductors = 1;
    request.meanTurnLength = units::Length{0.2};
    request.bareConductorCrossSection = units::Area{1.0e-6};
    request.windingTemperature = units::TemperatureKelvin{293.15};
    request.harmonicAnalysis.branchPoints = {
        {17, 1, units::Frequency{50.0}, 1.0,
            {std::complex<double>{1.0, 0.0},
             std::complex<double>{0.0, 2.0},
             std::complex<double>{-3.0, 0.0}}},
        {17, 5, units::Frequency{250.0}, 0.5,
            {std::complex<double>{0.5, 0.0},
             std::complex<double>{0.0, 0.5},
             std::complex<double>{-0.5, 0.0}}}
    };

    const auto result = calculateRevisionBoundWindingCopperLoss(store, request);
    assert(result);
    assert(result.value().conductorMaterialRevision.revisionId == "SEED-CU-C11000-R1");
    assert(result.value().conductorMaterialRevision.material.id == "MAT-CU-C11000-ETP");
    const double resistance = 1.707e-8 * 10.0 * 0.2 / 1.0e-6;
    assert(std::abs(result.value().copperLoss.resistanceAtOperatingTemperature.si() -
                    resistance) < 1.0e-12);
    assert(std::abs(result.value().copperLoss.harmonicLosses[0].totalThreePhaseLoss.si() -
                    14.0 * resistance) < 1.0e-12);
    assert(std::abs(result.value().copperLoss.harmonicLosses[1].totalThreePhaseLoss.si() -
                    0.75 * resistance) < 1.0e-12);

    auto missingBranch = request;
    missingBranch.filterBranchId = 99;
    const auto missing = calculateRevisionBoundWindingCopperLoss(store, missingBranch);
    assert(!missing && missing.error().code == ErrorCode::MissingRequiredData);

    auto wrongRevision = request;
    wrongRevision.conductorMaterialRevisionId = "SEED-TDK-N87-R1";
    const auto wrongType = calculateRevisionBoundWindingCopperLoss(store, wrongRevision);
    assert(!wrongType && wrongType.error().code == ErrorCode::InvalidInput);

    database.close();
    std::filesystem::remove(databasePath);
    return 0;
}
