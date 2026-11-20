#include "p135/infrastructure/sqlite/revision_bound_physical_filter_magnetic.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include <array>
#include <cassert>
#include <complex>
#include <filesystem>
#include <string>

using namespace p135;
using namespace p135::application;
using namespace p135::infrastructure::sqlite;

namespace {

domain::ThreeLimbCorePhysicalGeometry validCore()
{
    domain::ThreeLimbCorePhysicalGeometry g;
    g.limbWidths = {units::Length{0.04}, units::Length{0.04}, units::Length{0.04}};
    g.limbDepths = {units::Length{0.08}, units::Length{0.08}, units::Length{0.08}};
    g.windowWidths = {units::Length{0.06}, units::Length{0.06}};
    g.windowHeights = {units::Length{0.12}, units::Length{0.12}};
    g.yokeThicknesses.fill(units::Length{0.04});
    g.yokeDepths.fill(units::Length{0.08});
    g.limbCenterSpacings = {
        units::Length{0.10}, units::Length{0.10}, units::Length{0.20}
    };
    g.coreOverallHeight = units::Length{0.20};
    g.coreOverallWidth = units::Length{0.24};
    g.laminationConstruction = "explicit integration-test geometry";
    for (const auto interface : {
             domain::CoreJointInterface::TopYokeABToLimbA,
             domain::CoreJointInterface::TopYokeABToLimbB,
             domain::CoreJointInterface::TopYokeBCToLimbB,
             domain::CoreJointInterface::TopYokeBCToLimbC,
             domain::CoreJointInterface::BottomYokeABToLimbA,
             domain::CoreJointInterface::BottomYokeABToLimbB,
             domain::CoreJointInterface::BottomYokeBCToLimbB,
             domain::CoreJointInterface::BottomYokeBCToLimbC}) {
        g.joints.push_back({interface, domain::CoreJointType::Butt, {}});
    }
    return g;
}

PhysicalFilterReactorMagneticRequest makeRequest()
{
    PhysicalFilterReactorMagneticRequest request;
    request.physicalCore = validCore();
    request.stackingFactor = 0.95;
    auto& filterRequest = request.filterMagnetic;
    filterRequest.filterBranchId = 7;
    filterRequest.maximumSamples = 144;
    filterRequest.harmonicAnalysis.branchPoints.push_back(filter::BranchHarmonicPoint{
        7, 1, units::Frequency{10000.0}, 1.0,
        std::array<std::complex<double>, 3>{{
            {1.0, 0.0}, {-0.5, -0.8660254037844386}, {-0.5, 0.8660254037844386}
        }}
    });
    filterRequest.magnetic.turns = {100, 100, 100};

    // This deliberately bogus value proves the adapter always binds the exact
    // selected SQLite revision rather than trusting caller-supplied material.
    filterRequest.magnetic.material.id = "CALLER-SUPPLIED-PLACEHOLDER";
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    assert(argc == 3);
    const auto databasePath =
        (std::filesystem::temp_directory_path() /
         "project135_revision_bound_magnetic_test.sqlite").string();
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
        store, "revision-bound-magnetic-test", "2026-11-20T00:00:00Z");
    assert(seeded && seeded.value() > 0);

    const auto result = analyzePhysicalFilterReactorMagneticCycleWithMaterialRevision(
        store, "SEED-TDK-N87-R1", makeRequest());
    assert(result);
    assert(result.value().materialRevision.revisionId == "SEED-TDK-N87-R1");
    assert(result.value().materialRevision.material.id == "MAT-TDK-N87");
    assert(result.value().materialRevision.sourceMaterialId == "TDK-N87");
    assert(result.value().materialRevision.bhCurveStatus == "APPROXIMATE_DIGITIZATION");
    assert(result.value().magneticAnalysis.magneticAnalysis.filterBranchId == 7);
    assert(result.value().magneticAnalysis.magneticAnalysis.excitations.size() == 1);
    assert(result.value().magneticAnalysis.effectiveMagneticGeometry.limbEffectiveAreas[0]);

    const auto incompleteSteel = analyzePhysicalFilterReactorMagneticCycleWithMaterialRevision(
        store, "SEED-TK-M235-35A-R1", makeRequest());
    assert(!incompleteSteel);
    assert(incompleteSteel.error().code == ErrorCode::InvalidInput);

    const auto missingRevision = analyzePhysicalFilterReactorMagneticCycleWithMaterialRevision(
        store, "NO-SUCH-REVISION", makeRequest());
    assert(!missingRevision);
    assert(missingRevision.error().code == ErrorCode::MissingRequiredData);

    database.close();
    std::filesystem::remove(databasePath);
    return 0;
}
