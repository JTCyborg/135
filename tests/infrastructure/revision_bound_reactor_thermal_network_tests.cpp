#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/revision_bound_reactor_thermal_network.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"
#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <complex>
#include <filesystem>
#include <string>

using namespace p135;
using namespace p135::infrastructure::sqlite;

int main(int argc, char** argv)
{
    assert(argc == 3);
    const auto databasePath =
        (std::filesystem::temp_directory_path() /
         "project135_revision_bound_reactor_thermal_network_test.sqlite").string();
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
        store, "revision-bound-reactor-thermal-test", "2026-11-20T00:00:00Z");
    assert(seeded && seeded.value() > 0);

    domain::ThreeLimbCoreLossRequest coreLoss;
    coreLoss.frequency = units::Frequency{25000.0};
    for (std::size_t i = 0; i < 7; ++i) {
        coreLoss.peakFluxDensities[i] = units::FluxDensity{0.2};
        coreLoss.temperatures[i] = units::TemperatureKelvin{373.15};
        coreLoss.regionMasses[i] = units::Mass{1.0};
        coreLoss.regionMassSourceReferences[i] = "weighed-core-region-" + std::to_string(i);
    }

    RevisionBoundWindingCopperLossRequest windingLoss;
    windingLoss.conductorMaterialRevisionId = "SEED-CU-C11000-R1";
    windingLoss.filterBranchId = 17;
    windingLoss.turns = 10;
    windingLoss.parallelConductors = 1;
    windingLoss.meanTurnLength = units::Length{0.2};
    windingLoss.bareConductorCrossSection = units::Area{1.0e-6};
    windingLoss.windingTemperature = units::TemperatureKelvin{293.15};
    windingLoss.harmonicAnalysis.branchPoints = {
        {17, 1, units::Frequency{50.0}, 1.0,
            {std::complex<double>{1.0, 0.0},
             std::complex<double>{0.0, 2.0},
             std::complex<double>{-3.0, 0.0}}}
    };

    thermal::ReactorThermalNetworkConfiguration configuration;
    configuration.ambientTemperature = units::TemperatureKelvin{293.15};
    for (std::size_t i = 0; i < 7; ++i) {
        configuration.coreMaximumTemperatures[i] = units::TemperatureKelvin{400.0};
        configuration.coreTemperatureLimitSourceReferences[i] = "core-limit-datasheet-R1";
    }
    for (std::size_t i = 0; i < 3; ++i) {
        configuration.windingMaximumTemperatures[i] = units::TemperatureKelvin{400.0};
        configuration.windingTemperatureLimitSourceReferences[i] = "winding-limit-datasheet-R1";
    }
    const std::array<std::string, 10> nodeIds{
        "core.limb_a", "core.limb_b", "core.limb_c",
        "core.yoke_top_ab", "core.yoke_top_bc",
        "core.yoke_bottom_ab", "core.yoke_bottom_bc",
        "winding.phase_a", "winding.phase_b", "winding.phase_c"
    };
    for (const auto& id : nodeIds)
        configuration.ambientLinks.push_back({id, 1.0, "measured-thermal-path-R1"});
    configuration.links.push_back({
        "core.limb_a", "winding.phase_a", 0.5, "measured-core-winding-coupling-R1"
    });

    const auto result = analyzeRevisionBoundReactorThermalNetwork(
        store, "SEED-TDK-N87-R1", coreLoss, windingLoss, configuration);
    assert(result);
    assert(result.value().coreMaterialRevision.revisionId == "SEED-TDK-N87-R1");
    assert(result.value().conductorMaterialRevision.revisionId == "SEED-CU-C11000-R1");
    assert(!result.value().coreMaterialRevision.contentHash.empty());
    assert(!result.value().conductorMaterialRevision.contentHash.empty());
    assert(result.value().thermal.network.solved);
    assert(result.value().thermal.network.nodes.size() == 10);
    assert(result.value().thermal.network.nodes[0].loss.si() > 0.0);
    assert(result.value().thermal.phaseWindingLosses[0].si() > 0.0);
    assert(!result.value().thermal.network.modelProvenanceComplete);
    for (const auto& node : result.value().thermal.network.nodes)
        assert(node.status == thermal::AssessmentStatus::NotVerified);

    // Even if a reviewer toggles the material release flag, approximate
    // core-loss digitization must never be promoted to a verified thermal result.
    const auto baseCoreRevision = store.findRevision("SEED-TDK-N87-R1");
    assert(baseCoreRevision && baseCoreRevision.value().has_value());
    auto approximateCoreRevision = *baseCoreRevision.value();
    approximateCoreRevision.revisionId = "TEST-N87-APPROXIMATE-LOSS-VERIFIED-FLAG";
    approximateCoreRevision.materialId = "TEST-N87-APPROXIMATE-LOSS";
    approximateCoreRevision.revisionNumber = 1;
    auto approximatePayload = nlohmann::json::parse(approximateCoreRevision.payload);
    approximatePayload["dataQuality"]["coreLossStatus"] = "APPROXIMATE_DIGITIZATION";
    approximatePayload["dataQuality"]["designReleaseStatus"] = "VERIFIED";
    const auto approximateCanonical = canonicalizeJson(approximatePayload.dump());
    assert(approximateCanonical);
    const auto approximateHash = sha256Hex(approximateCanonical.value());
    assert(approximateHash);
    approximateCoreRevision.payload = approximateCanonical.value();
    approximateCoreRevision.contentHash = approximateHash.value();
    assert(store.appendRevision(approximateCoreRevision));
    const auto approximateThermal = analyzeRevisionBoundReactorThermalNetwork(
        store, approximateCoreRevision.revisionId, coreLoss, windingLoss, configuration);
    assert(approximateThermal);
    assert(approximateThermal.value().coreMaterialRevision.designReleaseStatus == "VERIFIED");
    assert(approximateThermal.value().coreMaterialRevision.coreLossStatus ==
           "APPROXIMATE_DIGITIZATION");
    assert(!approximateThermal.value().thermal.network.modelProvenanceComplete);
    for (const auto& node : approximateThermal.value().thermal.network.nodes)
        assert(node.status == thermal::AssessmentStatus::NotVerified);

    auto missingCore = analyzeRevisionBoundReactorThermalNetwork(
        store, "NO-SUCH-CORE-REVISION", coreLoss, windingLoss, configuration);
    assert(!missingCore && missingCore.error().code == ErrorCode::MissingRequiredData);

    auto missingConductor = windingLoss;
    missingConductor.conductorMaterialRevisionId = "NO-SUCH-CONDUCTOR-REVISION";
    const auto missingWire = analyzeRevisionBoundReactorThermalNetwork(
        store, "SEED-TDK-N87-R1", coreLoss, missingConductor, configuration);
    assert(!missingWire && missingWire.error().code == ErrorCode::MissingRequiredData);

    database.close();
    std::filesystem::remove(databasePath);
    return 0;
}
