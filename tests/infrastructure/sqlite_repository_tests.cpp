#include "p135/infrastructure/sqlite/canonical_json.hpp"
#include "p135/infrastructure/sqlite/migration_runner.hpp"
#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"
#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"
#include "p135/application/material_revision_use_cases.hpp"

#include <cassert>
#include <filesystem>
#include <string>

using namespace p135::infrastructure::sqlite;
using p135::application::MaterialRevision;
using p135::application::CompareMaterialRevisions;
using p135::application::RestoreMaterialRevision;

int main(int argc, char** argv)
{
    assert(argc == 3);
    const auto canonical = canonicalizeJson("{ \"b\": 2, \"a\": 1 }");
    assert(canonical && canonical.value() == "{\"a\":1,\"b\":2}");
    assert(!canonicalizeJson("{"));
    const auto knownHash = sha256Hex("{\"a\":1}");
    assert(knownHash &&
        knownHash.value() == "015abd7f5cc57a2dd94b7590f04ad8084273905ee33ec5cebeae62276a97f862");
    const auto databasePath = std::filesystem::temp_directory_path() /
        "project135_sqlite_material_store_tests.db";
    std::error_code ignored;
    std::filesystem::remove(databasePath, ignored);

    {
        SqliteDatabase database;
        assert(database.open(databasePath.string()));
        assert(database.execute(
            "CREATE TABLE schema_migrations ("
            "version INTEGER PRIMARY KEY, name TEXT NOT NULL, "
            "checksum_sha256 TEXT NOT NULL, applied_at_utc TEXT NOT NULL);"));
        MigrationRunner runner(database);
        const Migration migration{1, "initial_material_revision_store", argv[1]};
        assert(runner.apply(migration));
        assert(runner.apply(migration));
        const Migration calculationMigration{2, "calculation_snapshots", argv[2]};
        assert(runner.apply(calculationMigration));
        assert(runner.apply(calculationMigration));

        SqliteMaterialRevisionStore store(database);
        MaterialRevision first{
            "REV-1", "MAT-TEST", 1, std::nullopt, "core-material", "{}",
            "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
            "test://datasheet", "dataset-1", "test-author", "2026-01-01T00:00:00Z"
        };
        assert(store.appendRevision(first));
        const auto initiallyArchived = store.isArchived("MAT-TEST");
        assert(initiallyArchived && !initiallyArchived.value());

        const auto found = store.findRevision("REV-1");
        assert(found && found.value());
        assert(found.value()->payload == "{}");
        assert(found.value()->contentHash == first.contentHash);
        const auto latest = store.latestRevision("MAT-TEST");
        assert(latest && latest.value() && latest.value()->revisionNumber == 1);

        auto invalidHash = first;
        invalidHash.revisionId = "REV-BAD";
        invalidHash.contentHash = std::string(64, '0');
        assert(!store.appendRevision(invalidHash));

        auto nonCanonical = first;
        nonCanonical.revisionId = "REV-WHITESPACE";
        nonCanonical.payload = "{ }";
        nonCanonical.contentHash = sha256Hex(nonCanonical.payload).value();
        assert(!store.appendRevision(nonCanonical));

        MaterialRevision second{
            "REV-2", "MAT-TEST", 2, std::string("REV-1"), "core-material", "{\"a\":1}",
            "015abd7f5cc57a2dd94b7590f04ad8084273905ee33ec5cebeae62276a97f862",
            "test://datasheet", "dataset-2", "test-author", "2026-01-02T00:00:00Z"
        };
        assert(store.appendRevision(second));
        const auto latestSecond = store.latestRevision("MAT-TEST");
        assert(latestSecond && latestSecond.value() && latestSecond.value()->revisionNumber == 2);
        const auto history = store.revisionHistory("MAT-TEST");
        assert(history && history.value().size() == 2);
        assert(history.value()[0].revisionNumber == 1 && history.value()[1].revisionNumber == 2);
        MaterialCatalogFilter catalogFilter;
        catalogFilter.searchText = "mat-test";
        const auto catalog = store.searchLatestRevisions(catalogFilter);
        assert(catalog && catalog.value().size() == 1);
        assert(catalog.value()[0].revisionId == "REV-2");
        catalogFilter.pageOffset = 1;
        const auto nextPage = store.searchLatestRevisions(catalogFilter);
        assert(nextPage && nextPage.value().empty());
        catalogFilter.pageSize = 0;
        assert(!store.searchLatestRevisions(catalogFilter));

        assert(store.setArchived("MAT-TEST", true, "test-author", "2026-01-03T00:00:00Z"));
        const auto archivedState = store.isArchived("MAT-TEST");
        assert(archivedState && archivedState.value());
        MaterialCatalogFilter activeOnly;
        assert(store.searchLatestRevisions(activeOnly).value().empty());
        activeOnly.includeArchived = true;
        assert(store.searchLatestRevisions(activeOnly).value().size() == 1);
        assert(!store.appendRevision(MaterialRevision{
            "REV-3", "MAT-TEST", 3, std::string("REV-2"), "core-material", "{}",
            first.contentHash, "test://datasheet", "dataset-3", "test-author",
            "2026-01-04T00:00:00Z"
        }));
        assert(store.setArchived("MAT-TEST", false, "test-author", "2026-01-05T00:00:00Z"));
        const auto activeState = store.isArchived("MAT-TEST");
        assert(activeState && !activeState.value());

        assert(store.recordUsage(MaterialUsageReference{
            "USE-1", "REV-1", MaterialUsageKind::Product, "PRODUCT-1",
            "2026-01-06T00:00:00Z", "test-author"
        }));

        const std::string calculationPayload =
            "{\"engineVersions\":{\"filter\":\"harmonics-v1\",\"thermal\":\"thermal-v1\"},"
            "\"input\":{\"frequencyHz\":50,\"voltageV\":400},"
            "\"result\":{\"status\":\"NOT_VERIFIED\"}}";
        const auto calculationHash = sha256Hex(calculationPayload);
        assert(calculationHash);
        CalculationRecord calculation{
            "CALC-1", "PROJECT-1", "Integration test project", "shunt-passive-filter",
            "{\"frequencyHz\":50,\"voltageV\":400}",
            "{\"status\":\"NOT_VERIFIED\"}",
            "{\"filter\":\"harmonics-v1\",\"thermal\":\"thermal-v1\"}",
            calculationHash.value(), "test-author", "2026-01-06T00:00:01Z"
        };
        assert(store.recordCalculation(calculation, {
            {"USE-2", "REV-2"}
        }));
        const auto savedCalculation = store.findCalculation("CALC-1");
        assert(savedCalculation && savedCalculation.value());
        assert(savedCalculation.value()->contentHash == calculation.contentHash);
        assert(savedCalculation.value()->inputJson == calculation.inputJson);
        const auto calculationLinks = store.calculationMaterials("CALC-1");
        assert(calculationLinks && calculationLinks.value().size() == 1);
        assert(calculationLinks.value().front().revisionId == "REV-2");
        const auto calculationHistory = store.listCalculations();
        assert(calculationHistory && calculationHistory.value().size() == 1);
        assert(calculationHistory.value().front().calculationId == "CALC-1");
        const auto searchedHistory = store.listCalculations("Integration test");
        assert(searchedHistory && searchedHistory.value().size() == 1);
        assert(store.listCalculations("no-match").value().empty());
        assert(!store.listCalculations({}, 0, 0));
        assert(!store.findCalculation("MISSING-CALC").value());

        auto brokenCalculation = calculation;
        brokenCalculation.calculationId = "CALC-ROLLBACK";
        assert(!store.recordCalculation(brokenCalculation, {
            {"USE-ROLLBACK", "REV-1"}, {"USE-MISSING", "REV-MISSING"}
        }));
        assert(!store.findCalculation("CALC-ROLLBACK").value());
        const auto inUse = store.isMaterialInUse("MAT-TEST");
        assert(inUse && inUse.value());
        assert(!store.setArchived("MAT-TEST", true, "test-author", "2026-01-07T00:00:00Z"));
        assert(!database.execute("UPDATE material_revisions SET payload_json='changed';"));
        CompareMaterialRevisions compare(store);
        const auto comparison = compare.execute("REV-1", "REV-2");
        assert(comparison);
        assert(!comparison.value().payloadEqual);
        assert(comparison.value().recordTypeEqual);

        RestoreMaterialRevision restore(store);
        const auto restored = restore.execute(
            "REV-1", "REV-3", "integration-test", "2026-01-08T00:00:00Z");
        assert(restored);
        assert(restored.value().revisionNumber == 3);
        assert(restored.value().parentRevisionId == std::optional<std::string>{"REV-2"});
        assert(restored.value().payload == first.payload);
        assert(restored.value().sourceReference == "restore:REV-1");
        const auto restoredLatest = store.latestRevision("MAT-TEST");
        assert(restoredLatest && restoredLatest.value());
        assert(restoredLatest.value()->revisionId == "REV-3");
        const auto restoredHistory = store.revisionHistory("MAT-TEST");
        assert(restoredHistory && restoredHistory.value().size() == 3);
        const auto seeded = seedStarterMaterialCatalog(store, "integration-test", "2026-11-19T00:00:00Z");
        assert(seeded && seeded.value() == 203);
        const auto seededAgain = seedStarterMaterialCatalog(store, "integration-test", "2026-11-19T00:00:01Z");
        assert(seededAgain && seededAgain.value() == 0);
        MaterialCatalogFilter ferriteCoreComponents;
        ferriteCoreComponents.recordType = "core-component";
        const auto ferriteCoreCatalog = store.searchLatestRevisions(ferriteCoreComponents);
        assert(ferriteCoreCatalog && ferriteCoreCatalog.value().size() == 74);
        const auto pqCore = store.latestRevision("COMP-TDK-B65879B0000R087");
        assert(pqCore && pqCore.value());
        assert(pqCore.value()->payload.find("PQ 32/30") != std::string::npos);
        assert(pqCore.value()->payload.find("4800") != std::string::npos);
        assert(pqCore.value()->payload.find("N87") != std::string::npos);
        MaterialCatalogFilter coreMaterials;
        coreMaterials.recordType = "core-material";
        const auto coreCatalog = store.searchLatestRevisions(coreMaterials);
        // MAT-TEST, two ferrite materials, 31 non-oriented and 17 grain-oriented steel grades.
        assert(coreCatalog && coreCatalog.value().size() == 51);
        const auto n87 = store.latestRevision("MAT-TDK-N87");
        assert(n87 && n87.value());
        assert(n87.value()->payload.find("APPROXIMATE_DIGITIZATION") != std::string::npos);
        const auto n97 = store.latestRevision("MAT-TDK-N97");
        assert(n97 && n97.value());
        const auto copper = store.latestRevision("MAT-CU-C11000-ETP");
        assert(copper && copper.value());

        assert(copper.value()->payload.find("1.707e-08") != std::string::npos);
        const auto p180 = store.latestRevision("MAT-ELEKTRISOLA-POLYSOL-180");
        assert(p180 && p180.value());
        assert(p180.value()->payload.find("thermalClass_C") != std::string::npos);
        assert(p180.value()->payload.find("0.0005") != std::string::npos);
        const auto ai210 = store.latestRevision("MAT-ELEKTRISOLA-AMIDESTER-AI210");
        assert(ai210 && ai210.value());
        assert(ai210.value()->payload.find("polyamideimide") != std::string::npos);

        const auto aluminium1350 = store.latestRevision("MAT-AL-1350-H0");
        assert(aluminium1350 && aluminium1350.value());
        assert(aluminium1350.value()->payload.find("63.3") != std::string::npos);
        const auto aluminium6101 = store.latestRevision("MAT-AL-6101-T4");
        assert(aluminium6101 && aluminium6101.value());
        assert(aluminium6101.value()->payload.find("49.2") != std::string::npos);
        const auto cca10 = store.latestRevision("MAT-CCA-10");
        assert(cca10 && cca10.value());
        assert(cca10.value()->payload.find("62.854") != std::string::npos);
        const auto cca15 = store.latestRevision("MAT-CCA-15");
        assert(cca15 && cca15.value());
        assert(cca15.value()->payload.find("64.416") != std::string::npos);
        const auto copperTube = store.latestRevision("FORM-CU-C11000-tube");
        assert(copperTube && copperTube.value());
        assert(copperTube.value()->payload.find("hollow-circle") != std::string::npos);
        const auto aluminiumBusbar = store.latestRevision("FORM-AL-6101-T4-flat-bar-busbar");
        assert(aluminiumBusbar && aluminiumBusbar.value());
        assert(aluminiumBusbar.value()->payload.find("PARAMETRIC_USER_INPUT") != std::string::npos);
        const auto ccaWire = store.latestRevision("FORM-CCA-15-round-wire");
        assert(ccaWire && ccaWire.value());
        assert(ccaWire.value()->payload.find("circle") != std::string::npos);
        MaterialCatalogFilter conductorProfiles;
        conductorProfiles.recordType = "conductor-stock-profile";
        const auto conductorProfileCatalog = store.searchLatestRevisions(conductorProfiles);
        assert(conductorProfileCatalog && conductorProfileCatalog.value().size() == 46);
        const auto copperFoil = store.latestRevision("FORM-CU-C11000-foil");
        assert(copperFoil && copperFoil.value());
        assert(copperFoil.value()->payload.find("PARAMETRIC_USER_INPUT") != std::string::npos);
        const auto aluminiumFoil = store.latestRevision("FORM-AL-1350-H0-foil");
        assert(aluminiumFoil && aluminiumFoil.value());
        const auto cooledConductor = store.latestRevision("FORM-CU-C11000-hollow-rectangular-water-cooled");
        assert(cooledConductor && cooledConductor.value());
        assert(cooledConductor.value()->payload.find("hollow-rectangle") != std::string::npos);
        const auto litz = store.latestRevision("ASM-CU-LITZ-ROUND");
        assert(litz && litz.value());
        assert(litz.value()->payload.find("NOT_MODELED") != std::string::npos);
        const auto ctc = store.latestRevision("ASM-CU-CTC");
        assert(ctc && ctc.value());
        MaterialCatalogFilter conductorAssemblies;
        conductorAssemblies.recordType = "conductor-assembly";
        const auto assemblyCatalog = store.searchLatestRevisions(conductorAssemblies);
        assert(assemblyCatalog && assemblyCatalog.value().size() == 4);
        const auto goSteel = store.latestRevision("MAT-TK-POWERCORE-H080-23");
        assert(goSteel && goSteel.value());
        assert(goSteel.value()->payload.find("0.00023") != std::string::npos);
        assert(goSteel.value()->payload.find("1.7") != std::string::npos);
        const auto steel = store.latestRevision("MAT-TK-POWERCORE-M235-35A");
        assert(steel && steel.value());
        assert(steel.value()->payload.find("2.35") != std::string::npos);
        assert(steel.value()->payload.find("NOT_PUBLISHED_IN_IMPORTED_DATASET") != std::string::npos);

        const auto steel65 = store.latestRevision("MAT-TK-POWERCORE-M1000-65A");
        assert(steel65 && steel65.value());
        assert(steel65.value()->payload.find("0.00065") != std::string::npos);
        assert(steel65.value()->payload.find("10.0") != std::string::npos);
        const auto steel50 = store.latestRevision("MAT-TK-POWERCORE-M940-50A");
        assert(steel50 && steel50.value());
        assert(steel50.value()->payload.find("9.4") != std::string::npos);
        const auto etdCore = store.latestRevision("COMP-TDK-B66363G0000X187");
        assert(etdCore && etdCore.value());
        assert(etdCore.value()->payload.find("ETD 39/20/13") != std::string::npos);
        assert(etdCore.value()->payload.find("2700") != std::string::npos);
        const auto etdGapped = store.latestRevision("COMP-TDK-B66358G0200X187");
        assert(etdGapped && etdGapped.value());
        assert(etdGapped.value()->payload.find("0.2") != std::string::npos);
        const auto kapton = store.latestRevision("MAT-DUPONT-KAPTON-HN-25UM");
        assert(kapton && kapton.value());
        assert(kapton.value()->payload.find("303000000") != std::string::npos);
        assert(kapton.value()->payload.find("0.000025") != std::string::npos);
        const auto glassTape = store.latestRevision("COMP-3M-SCOTCH-27");
        assert(glassTape && glassTape.value());
        assert(glassTape.value()->payload.find("0.000177") != std::string::npos);
        assert(glassTape.value()->payload.find("3000") != std::string::npos);
        MaterialCatalogFilter insulation;
        insulation.recordType = "insulation-material";
        const auto insulationCatalog = store.searchLatestRevisions(insulation);
        assert(insulationCatalog && insulationCatalog.value().size() == 11);
        const auto nomex50 = store.latestRevision("MAT-DUPONT-NOMEX-410-50UM");
        assert(nomex50 && nomex50.value());
        assert(nomex50.value()->payload.find("18000000") != std::string::npos);
        const auto nomex380 = store.latestRevision("MAT-DUPONT-NOMEX-410-380UM");
        assert(nomex380 && nomex380.value());
        assert(nomex380.value()->payload.find("33000000") != std::string::npos);
        const auto nomex130 = store.latestRevision("MAT-DUPONT-NOMEX-410-130UM");
        assert(nomex130 && nomex130.value());
        assert(nomex130.value()->payload.find("28000000") != std::string::npos);
        const auto kapton125 = store.latestRevision("MAT-DUPONT-KAPTON-HN-125UM");
        assert(kapton125 && kapton125.value());
        assert(kapton125.value()->payload.find("315000000") != std::string::npos);
        const auto kapton50 = store.latestRevision("MAT-DUPONT-KAPTON-HN-50UM");
        assert(kapton50 && kapton50.value());
        assert(kapton50.value()->payload.find("240000000") != std::string::npos);
        const auto isomid = store.latestRevision("MAT-ALLEIMA-ISOMID");
        assert(isomid && isomid.value());
        assert(isomid.value()->payload.find("200") != std::string::npos);
        const auto up142 = store.latestRevision("MAT-ELANTAS-UP142-TX");
        assert(up142 && up142.value());
        assert(up142.value()->payload.find("20000") != std::string::npos);
        const auto elmoglas = store.latestRevision("MAT-ELANTAS-ELMOGLAS-H69A");
        assert(elmoglas && elmoglas.value());
        assert(elmoglas.value()->payload.find("180") != std::string::npos);
        MaterialCatalogFilter conductors;
        conductors.recordType = "conductor-material";
        const auto conductorCatalog = store.searchLatestRevisions(conductors);
        assert(conductorCatalog && conductorCatalog.value().size() == 15);
        MaterialCatalogFilter capacitors;
        capacitors.recordType = "capacitor";
        const auto capacitorCatalog = store.searchLatestRevisions(capacitors);
        assert(capacitorCatalog && capacitorCatalog.value().size() == 2);
        const auto filmCap = store.latestRevision("COMP-TDK-B32776H1335K000");
        assert(filmCap && filmCap.value());
        assert(filmCap.value()->payload.find("1100") != std::string::npos);
        assert(filmCap.value()->payload.find("0.0228") != std::string::npos);
        MaterialCatalogFilter resistors;
        resistors.recordType = "resistor";
        const auto resistorCatalog = store.searchLatestRevisions(resistors);
        assert(resistorCatalog && resistorCatalog.value().size() == 1);
        const auto rh50 = store.latestRevision("COMP-VISHAY-RH50");
        assert(rh50 && rh50.value());
        assert(rh50.value()->payload.find("1285") != std::string::npos);

    }

    {
        SqliteDatabase reopened;
        assert(reopened.open(databasePath.string()));
        assert(reopened.execute("SELECT 1;"));
    }

    std::filesystem::remove(databasePath, ignored);
    return 0;
}
