#include "p135/infrastructure/sqlite/core_material_revision_loader.hpp"
#include "p135/domain/core_loss.hpp"
#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include <array>
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

    // User-imported core-loss data retains explicit provenance and converts
    // volumetric W/m^3 to the solver's mass-specific W/kg using this revision's density.
    const auto n87Revision = store.findRevision("SEED-TDK-N87-R1");
    assert(n87Revision && n87Revision.value().has_value());
    auto importedLossRevision = *n87Revision.value();
    importedLossRevision.revisionId = "TEST-TDK-N87-CORE-LOSS-IMPORT";
    importedLossRevision.materialId = "TEST-TDK-N87-CORE-LOSS";
    importedLossRevision.revisionNumber = 1;
    auto importedLossPayload = nlohmann::json::parse(importedLossRevision.payload);
    importedLossPayload["coreLossData"]["points"] = nlohmann::json::array({
        {{"frequency_Hz",50.0},{"peakFluxDensity_T",0.8},{"temperature_K",293.15},{"specificLoss_W_per_m3",10000.0}},
        {{"frequency_Hz",50.0},{"peakFluxDensity_T",0.8},{"temperature_K",333.15},{"specificLoss_W_per_m3",11000.0}},
        {{"frequency_Hz",50.0},{"peakFluxDensity_T",1.0},{"temperature_K",293.15},{"specificLoss_W_per_m3",20000.0}},
        {{"frequency_Hz",50.0},{"peakFluxDensity_T",1.0},{"temperature_K",333.15},{"specificLoss_W_per_m3",22000.0}},
        {{"frequency_Hz",100.0},{"peakFluxDensity_T",0.8},{"temperature_K",293.15},{"specificLoss_W_per_m3",30000.0}},
        {{"frequency_Hz",100.0},{"peakFluxDensity_T",0.8},{"temperature_K",333.15},{"specificLoss_W_per_m3",33000.0}},
        {{"frequency_Hz",100.0},{"peakFluxDensity_T",1.0},{"temperature_K",293.15},{"specificLoss_W_per_m3",60000.0}},
        {{"frequency_Hz",100.0},{"peakFluxDensity_T",1.0},{"temperature_K",333.15},{"specificLoss_W_per_m3",66000.0}}
    });
    importedLossPayload["coreLossProvenance"] = {
        {"sourceDocument","TEST-CORE-LOSS-DATASHEET-R1"},
        {"sourcePage","Table 4"},
        {"measurementMethod","Epstein frame test"},
        {"sampleDirection","Rolling direction"},
        {"waveform","SINUSOIDAL"},
        {"temperatureStatus","PER_POINT"},
        {"notes","E2E loader fixture"}
    };
    importedLossPayload["dataQuality"]["coreLossStatus"] = "MANUFACTURER_TABULATED";
    importedLossPayload["dataQuality"]["designReleaseStatus"] = "NOT_VERIFIED";
    const auto importedLossCanonical = canonicalizeJson(importedLossPayload.dump());
    assert(importedLossCanonical);
    const auto importedLossHash = sha256Hex(importedLossCanonical.value());
    assert(importedLossHash);
    importedLossRevision.payload = importedLossCanonical.value();
    importedLossRevision.contentHash = importedLossHash.value();
    assert(store.appendRevision(importedLossRevision));
    const auto loadedImportedLoss = loadCoreMaterialRevision(
        store, importedLossRevision.revisionId);
    assert(loadedImportedLoss);
    assert(loadedImportedLoss.value().material.lossData.size() == 8);
    assert(loadedImportedLoss.value().coreLossStatus == "MANUFACTURER_TABULATED");
    assert(loadedImportedLoss.value().coreLossSourceDocument == "TEST-CORE-LOSS-DATASHEET-R1");
    assert(loadedImportedLoss.value().coreLossSourcePage == "Table 4");
    assert(loadedImportedLoss.value().coreLossMeasurementMethod == "Epstein frame test");
    assert(loadedImportedLoss.value().coreLossSampleDirection == "Rolling direction");
    assert(loadedImportedLoss.value().coreLossWaveform == "SINUSOIDAL");
    assert(loadedImportedLoss.value().coreLossTemperatureStatus == "PER_POINT");
    assert(loadedImportedLoss.value().designReleaseStatus == "NOT_VERIFIED");
    assert(loadedImportedLoss.value().material.lossData.front().specificLoss.si() ==
           10000.0 / loadedImportedLoss.value().material.density.si());
    const auto interpolatedLoss = domain::evaluateCoreLossData(
        loadedImportedLoss.value().material, units::Frequency{75.0},
        units::FluxDensity{0.9}, units::TemperatureKelvin{313.15});
    assert(interpolatedLoss);
    assert(std::abs(interpolatedLoss.value().si() -
        (27750.0 / loadedImportedLoss.value().material.density.si())) < 1.0e-12);

    const auto n97 = loadCoreMaterialRevision(store, "SEED-TDK-N97-R1");
    assert(n97);
    assert(n97.value().material.id == "MAT-TDK-N97");
    assert(n97.value().material.bhCurve.size() == 8);
    assert(n97.value().material.lossData.size() == 4);

    // Published steel seed records do not contain a continuous B-H curve.
    // They must not be silently converted into solver-ready core materials.
    const auto incompleteSteel =
        loadCoreMaterialRevision(store, "SEED-TK-M235-35A-R1");
    assert(incompleteSteel);
    assert(incompleteSteel.value().material.bhCurve.empty());
    assert(incompleteSteel.value().bhCurveStatus == "NOT_PUBLISHED_IN_IMPORTED_DATASET");
    assert(incompleteSteel.value().designReleaseStatus == "NOT_VERIFIED");

    // The Kobe Steel table contains measured DC initial-magnetization data,
    // not an AC B-H loop or core-loss model. Preserve exactly that distinction.
    const auto steel35A300 =
        loadCoreMaterialRevision(store, "SEED-MAT-RESEARCH-35A300-DC-R1");
    assert(steel35A300);
    assert(steel35A300.value().material.dataSource == domain::MaterialDataSource::Measurement);
    assert(steel35A300.value().material.grade == "35A300 non-oriented electrical steel");
    assert(steel35A300.value().bhCurveStatus == "MEASURED_TABULAR");
    const std::array<double, 8> expectedH{
        100.0, 200.0, 300.0, 500.0, 1000.0, 2000.0, 5000.0, 20000.0
    };
    const std::array<double, 8> expected35A300B{
        1.20, 1.36, 1.42, 1.47, 1.53, 1.59, 1.70, 1.93
    };
    assert(steel35A300.value().material.bhCurve.size() == expectedH.size());
    for (std::size_t i = 0; i < expectedH.size(); ++i) {
        assert(steel35A300.value().material.bhCurve[i].fieldStrength.si() == expectedH[i]);
        assert(steel35A300.value().material.bhCurve[i].fluxDensity.si() == expected35A300B[i]);
    }
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
    const std::array<double, 8> expected50A800B{
        0.22, 0.99, 1.28, 1.45, 1.56, 1.63, 1.74, 1.97
    };
    assert(steel50A800.value().material.bhCurve.size() == expectedH.size());
    for (std::size_t i = 0; i < expectedH.size(); ++i) {
        assert(steel50A800.value().material.bhCurve[i].fieldStrength.si() == expectedH[i]);
        assert(steel50A800.value().material.bhCurve[i].fluxDensity.si() == expected50A800B[i]);
    }
    assert(steel50A800.value().material.lossData.empty());
    assert(steel50A800.value().coreLossStatus == "NOT_PUBLISHED");

    // Baosteel's official product page provides manufacturer-typical AC loss anchors.
    // Keep them separate from CoreMaterial::lossData because test temperature
    // is not published and the values are not guaranteed delivery limits.
    const auto baosteel35A300 = loadCoreLossReferenceDataset(
        store, "SEED-DATASET-BAOSTEEL-B35A300-AC-LOSS-R1");
    assert(baosteel35A300);
    assert(baosteel35A300.value().manufacturer == "Baoshan Iron & Steel Co., Ltd.");
    assert(baosteel35A300.value().grade == "B35A300 non-oriented electrical steel");
    assert(baosteel35A300.value().laminationThickness.si() == 0.00035);
    assert(baosteel35A300.value().density.si() == 7650.0);
    assert(baosteel35A300.value().minimumStackingFactor == 0.95);
    assert(baosteel35A300.value().points.size() == 4);
    assert(baosteel35A300.value().points[0].frequency.si() == 50.0);
    assert(baosteel35A300.value().points[0].peakFluxDensity.si() == 1.0);
    assert(baosteel35A300.value().points[0].specificLoss.si() == 1.14);
    assert(baosteel35A300.value().points[1].specificLoss.si() == 2.53);
    assert(baosteel35A300.value().points[2].frequency.si() == 60.0);
    assert(baosteel35A300.value().points[2].specificLoss.si() == 1.42);
    assert(baosteel35A300.value().points[3].specificLoss.si() == 3.13);
    for (const auto& point : baosteel35A300.value().points)
        assert(!point.measurementTemperature.has_value());
    assert(baosteel35A300.value().specificationLimit.has_value());
    assert(baosteel35A300.value().specificationLimit->maximumSpecificLoss.si() == 3.00);
    assert(baosteel35A300.value().qualityStatus ==
           "MANUFACTURER_TYPICAL_VALUES_NOT_GUARANTEED");
    assert(baosteel35A300.value().measurementMethod == "GB/T 3655-2008 magnetic measurement method; Epstein square-frame method");
    assert(baosteel35A300.value().temperatureStatus == "NOT_STATED_IN_SOURCE");
    assert(baosteel35A300.value().sampleDirection.find("Half horizontal and half vertical") !=
           std::string::npos);
    assert(baosteel35A300.value().supplementaryGraphSources.size() == 2);
    assert(baosteel35A300.value().supplementaryGraphSources[0].captureStatus ==
           "GRAPH_ONLY_NOT_DIGITIZED");
    assert(baosteel35A300.value().supplementaryGraphSources[0].sourceReference ==
           "https://esales.baosteel.com/portal/eleSteel/cn/download/B35A300.pdf");
    assert(baosteel35A300.value().supplementaryGraphSources[1].graphScope.find("2000 Hz") !=
           std::string::npos);
    assert(baosteel35A300.value().supplementaryGraphSources[1].captureStatus ==
           "GRAPH_ONLY_NOT_DIGITIZED");

    const auto exactLossAnchor = findExactCoreLossReferencePoint(
        baosteel35A300.value(), units::Frequency{50.0}, units::FluxDensity{1.0});
    assert(exactLossAnchor.has_value());
    assert(exactLossAnchor->specificLoss.si() == 1.14);
    assert(!exactLossAnchor->measurementTemperature.has_value());
    // A nearby operating point is deliberately not interpolated.
    assert(!findExactCoreLossReferencePoint(
        baosteel35A300.value(), units::Frequency{50.0},
        units::FluxDensity{1.000001}).has_value());
    assert(!findExactCoreLossReferencePoint(
        baosteel35A300.value(), units::Frequency{55.0},
        units::FluxDensity{1.0}).has_value());

    const auto baosteel50A800 = loadCoreLossReferenceDataset(
        store, "SEED-DATASET-BAOSTEEL-B50A800-AC-LOSS-R1");
    assert(baosteel50A800);
    assert(baosteel50A800.value().laminationThickness.si() == 0.00050);
    assert(baosteel50A800.value().density.si() == 7800.0);
    assert(baosteel50A800.value().minimumStackingFactor == 0.96);
    assert(baosteel50A800.value().points.size() == 4);
    assert(baosteel50A800.value().points[0].specificLoss.si() == 2.67);
    assert(baosteel50A800.value().points[1].specificLoss.si() == 5.70);
    assert(baosteel50A800.value().points[2].specificLoss.si() == 3.34);
    assert(baosteel50A800.value().points[3].specificLoss.si() == 7.11);
    assert(baosteel50A800.value().specificationLimit.has_value());
    assert(baosteel50A800.value().specificationLimit->maximumSpecificLoss.si() == 8.00);
    assert(baosteel50A800.value().supplementaryGraphSources.size() == 1);
    assert(baosteel50A800.value().supplementaryGraphSources.front().captureStatus ==
           "GRAPH_ONLY_NOT_DIGITIZED");
    assert(baosteel50A800.value().supplementaryGraphSources.front().sourceReference ==
           "https://esales.baosteel.com/portal/eleSteel/cn/download/B50A800.pdf");

    const auto wrongLossRecord = loadCoreLossReferenceDataset(store, "SEED-TDK-N87-R1");
    assert(!wrongLossRecord);
    assert(wrongLossRecord.error().code == ErrorCode::InvalidInput);

    // A duplicated frequency/flux coordinate is ambiguous and must not be
    // silently accepted as two independent measurements.
    const auto originalLossRevision = store.findRevision(
        "SEED-DATASET-BAOSTEEL-B35A300-AC-LOSS-R1");
    assert(originalLossRevision && originalLossRevision.value().has_value());

    auto invalidGraphPayload = nlohmann::json::parse(originalLossRevision.value()->payload);
    invalidGraphPayload["supplementaryGraphSources"][0]["captureStatus"] = "SOLVER_READY";
    const auto invalidGraphCanonical = canonicalizeJson(invalidGraphPayload.dump());
    assert(invalidGraphCanonical);
    const auto invalidGraphHash = sha256Hex(invalidGraphCanonical.value());
    assert(invalidGraphHash);
    auto invalidGraphRevision = *originalLossRevision.value();
    invalidGraphRevision.revisionId = "TEST-BAOSTEEL-B35A300-INVALID-GRAPH-STATUS";
    invalidGraphRevision.materialId = "TEST-BAOSTEEL-B35A300-INVALID-GRAPH";
    invalidGraphRevision.revisionNumber = 1;
    invalidGraphRevision.payload = invalidGraphCanonical.value();
    invalidGraphRevision.contentHash = invalidGraphHash.value();
    assert(store.appendRevision(invalidGraphRevision));
    const auto invalidGraphDataset = loadCoreLossReferenceDataset(
        store, invalidGraphRevision.revisionId);
    assert(!invalidGraphDataset);
    assert(invalidGraphDataset.error().code == ErrorCode::InvalidInput);

    auto duplicatePayload = nlohmann::json::parse(originalLossRevision.value()->payload);
    duplicatePayload["coreLossReferencePoints"].push_back(
        duplicatePayload["coreLossReferencePoints"].front());
    const auto duplicateCanonical = canonicalizeJson(duplicatePayload.dump());
    assert(duplicateCanonical);
    const auto duplicateHash = sha256Hex(duplicateCanonical.value());
    assert(duplicateHash);
    auto duplicateRevision = *originalLossRevision.value();
    duplicateRevision.revisionId = "TEST-BAOSTEEL-B35A300-DUPLICATE-POINTS";
    duplicateRevision.materialId = "TEST-BAOSTEEL-B35A300-DUPLICATE";
    duplicateRevision.revisionNumber = 1;
    duplicateRevision.payload = duplicateCanonical.value();
    duplicateRevision.contentHash = duplicateHash.value();
    assert(store.appendRevision(duplicateRevision));
    const auto duplicateDataset = loadCoreLossReferenceDataset(
        store, duplicateRevision.revisionId);
    assert(!duplicateDataset);
    assert(duplicateDataset.error().code == ErrorCode::InvalidInput);

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
