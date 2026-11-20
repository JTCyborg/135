#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace p135::infrastructure::sqlite {
namespace {

using nlohmann::json;
using application::MaterialRevision;

struct SeedRecord {
    std::string materialId;
    std::string revisionId;
    std::string recordType;
    std::string sourceReference;
    std::string datasetRevision;
    json payload;
};

Error seedError(std::string message)
{
    Error error;
    error.code = ErrorCode::InternalError;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "MATERIAL_CATALOG_SEED_FAILED", error.message,
        std::nullopt, std::nullopt, std::nullopt
    });
    return error;
}

std::vector<SeedRecord> starterRecords()
{
    const auto coreCurve = [](const std::vector<std::pair<double, double>>& points) {
        json result = json::array();
        for (const auto& [h, b] : points)
            result.push_back({{"fieldStrength_A_per_m", h}, {"fluxDensity_T", b}});
        return result;
    };

    const json n87 = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "TDK-N87"}, {"manufacturer", "TDK Electronics"},
            {"grade", "N87"}, {"family", "MnZn ferrite"},
            {"density_kg_per_m3", 4850.0}, {"initialPermeability_25C", 2200.0},
            {"saturationFluxDensity_25C_T", 0.490},
            {"saturationFluxDensity_100C_T", 0.390},
            {"coerciveField_25C_A_per_m", 21.0},
            {"coerciveField_100C_A_per_m", 13.0},
            {"curieTemperature_min_K", 483.15},
            {"recommendedFrequencyRange_Hz", {25000.0, 500000.0}}
        }},
        {"bhCurve", {
            {"curveType", "dynamic magnetization"},
            {"frequency_Hz", 10000.0}, {"temperature_K", 298.15},
            {"digitization", "Approximate points digitized from TDK graph FAL0599-4; typical curve, not raw tabular data."},
            {"points", coreCurve({
                {0.0, 0.0}, {20.0, 0.055}, {50.0, 0.135}, {100.0, 0.260},
                {200.0, 0.360}, {400.0, 0.430}, {800.0, 0.470}, {1200.0, 0.490}
            })}
        }},
        {"coreLossData", {
            {"dataKind", "manufacturer typical values"},
            {"points", json::array({
                {{"frequency_Hz", 25000.0}, {"peakFluxDensity_T", 0.200}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 57000.0}},
                {{"frequency_Hz", 100000.0}, {"peakFluxDensity_T", 0.200}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 375000.0}},
                {{"frequency_Hz", 300000.0}, {"peakFluxDensity_T", 0.100}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 390000.0}},
                {{"frequency_Hz", 500000.0}, {"peakFluxDensity_T", 0.050}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 215000.0}}
            })}
        }},
        {"dataQuality", {
            {"bhCurveStatus", "APPROXIMATE_DIGITIZATION"},
            {"coreLossStatus", "MANUFACTURER_TYPICAL_VALUES"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Use the loss points only at their stated frequency, peak flux density and temperature. Do not extrapolate them as a validated loss model."}
        }}
    };

    const json n97 = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "TDK-N97"}, {"manufacturer", "TDK Electronics"},
            {"grade", "N97"}, {"family", "MnZn ferrite"},
            {"density_kg_per_m3", 4850.0}, {"initialPermeability_25C", 2300.0},
            {"saturationFluxDensity_25C_T", 0.510},
            {"saturationFluxDensity_100C_T", 0.410},
            {"coerciveField_25C_A_per_m", 21.0},
            {"coerciveField_100C_A_per_m", 12.0},
            {"curieTemperature_min_K", 503.15},
            {"recommendedFrequencyRange_Hz", {25000.0, 500000.0}}
        }},
        {"bhCurve", {
            {"curveType", "dynamic magnetization"},
            {"frequency_Hz", 10000.0}, {"temperature_K", 298.15},
            {"digitization", "Approximate points digitized from TDK graph FAL0619-M; typical curve, not raw tabular data."},
            {"points", coreCurve({
                {0.0, 0.0}, {20.0, 0.058}, {50.0, 0.142}, {100.0, 0.275},
                {200.0, 0.385}, {400.0, 0.455}, {800.0, 0.490}, {1200.0, 0.510}
            })}
        }},
        {"coreLossData", {
            {"dataKind", "manufacturer typical values"},
            {"points", json::array({
                {{"frequency_Hz", 25000.0}, {"peakFluxDensity_T", 0.200}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 45000.0}},
                {{"frequency_Hz", 100000.0}, {"peakFluxDensity_T", 0.200}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 300000.0}},
                {{"frequency_Hz", 300000.0}, {"peakFluxDensity_T", 0.100}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 340000.0}},
                {{"frequency_Hz", 500000.0}, {"peakFluxDensity_T", 0.050}, {"temperature_K", 373.15}, {"specificLoss_W_per_m3", 205000.0}}
            })}
        }},
        {"dataQuality", {
            {"bhCurveStatus", "APPROXIMATE_DIGITIZATION"},
            {"coreLossStatus", "MANUFACTURER_TYPICAL_VALUES"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Use the loss points only at their stated frequency, peak flux density and temperature. Do not extrapolate them as a validated loss model."}
        }}
    };

    const json copper = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "CU-C11000-ETP"}, {"manufacturer", "Copper Development Association data"},
            {"grade", "C11000 ETP copper, annealed conductivity reference"},
            {"family", "high conductivity copper"},
            {"resistivityAt20C_ohm_m", 1.707e-8},
            {"temperatureCoefficientAt20C_per_K", 0.00393},
            {"density_kg_per_m3", 8910.0},
            {"conductivity_percent_IACS", 101.0}
        }},
        {"dataQuality", {
            {"resistivityStatus", "DERIVED_FROM_101_PERCENT_IACS"},
            {"densityStatus", "SOURCE_SPECIFIC_GRAVITY_8_91"},
            {"temperatureCoefficientStatus", "STANDARD_ENGINEERING_REFERENCE_VALUE"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Resistivity derived from the conventional 100% IACS reference resistivity 1.7241e-8 ohm m divided by 1.01. Actual wire temper, purity and processing can change properties."}
        }}
    };

    const json nomex = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "DUPONT-NOMEX-410"},
            {"manufacturer", "DuPont"}, {"grade", "Nomex 410, 0.25 mm (10 mil) reference"},
            {"family", "aramid insulation paper"},
            {"thermalClass", "R (220 C RTI electrical; verify application-specific system classification)"},
            {"maximumTemperature_K", 493.15},
            {"typicalAcRapidRiseDielectricStrength_V_per_m", 33000000.0},
            {"recommendedContinuousStress_V_per_m", 1600000.0},
            {"relativePermittivity_60Hz", 2.7},
            {"referenceThickness_m", 0.00025}
        }},
        {"dataQuality", {
            {"dielectricStrengthStatus", "TYPICAL_SHORT_TERM_TEST_VALUE"},
            {"continuousStressStatus", "MANUFACTURER_RECOMMENDATION"},
            {"temperatureStatus", "UL_RTI_ELECTRICAL_220C"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "The 33 kV/mm value is a typical AC rapid-rise test result, not a continuous design field. DuPont recommends continuous transformer stress not exceeding 1.6 kV/mm to reduce partial-discharge risk. Insulation-system qualification is still required."}
        }}
    };

    const json tdkCapSmall = {
        {"schemaVersion", 1},
        {"component", {
            {"id", "TDK-B32776H1335K000"}, {"manufacturer", "TDK Electronics"},
            {"series", "B3277*H"}, {"orderingCode", "B32776H1335K000"},
            {"technology", "metallized polypropylene film (MKP)"},
            {"application", "DC link"},
            {"capacitance_F", 3.3e-6}, {"capacitanceTolerance_fraction", 0.10},
            {"ratedVoltageDC_V", 1100.0}, {"maximumCaseTemperature_K", 378.15},
            {"equivalentSeriesResistance_ohm", 0.0228},
            {"equivalentSeriesInductance_H", 19.0e-9},
            {"construction", "radial boxed; 2 or 4 pins"},
            {"aecQ200", true}
        }},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_CATALOG_LISTING"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "ESR and ESL are catalog-listed typical values. Verify ripple-current, voltage derating, lifetime and thermal conditions against the current full datasheet before design release."}
        }}
    };

    const json tdkCapLarge = {
        {"schemaVersion", 1},
        {"component", {
            {"id", "TDK-B32778G4177K000"}, {"manufacturer", "TDK Electronics"},
            {"series", "B3277*D/E/G/J/T"}, {"orderingCode", "B32778G4177K000"},
            {"technology", "metallized polypropylene film (MKP)"},
            {"application", "DC link"},
            {"capacitance_F", 170.0e-6}, {"capacitanceTolerance_fraction", 0.10},
            {"ratedVoltageDC_V", 450.0}, {"maximumCaseTemperature_K", 378.15},
            {"equivalentSeriesResistance_ohm", 0.0026},
            {"equivalentSeriesInductance_H", 17.0e-9},
            {"construction", "radial boxed"},
            {"aecQ200", false}
        }},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_CATALOG_LISTING"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "ESR and ESL are catalog-listed typical values. Verify ripple-current, voltage derating, lifetime and thermal conditions against the current full datasheet before design release."}
        }}
    };

    const json vishayRh50 = {
        {"schemaVersion", 1},
        {"component", {
            {"id", "VISHAY-RH50-SERIES"}, {"manufacturer", "Vishay Sfernice"},
            {"series", "RH50"}, {"technology", "wirewound, aluminum-housed, chassis-mount"},
            {"ratedPowerAt25C_W", 50.0}, {"maximumWorkingVoltageRMS_V", 1285.0},
            {"resistanceRange_ohm", {0.01, 130000.0}},
            {"availableTolerance_percent", {0.5, 1.0, 2.0, 5.0, 10.0}},
            {"temperatureCoefficient_ppm_per_K", {
                {"below_5_ohm", 100.0}, {"5_to_10_ohm", 50.0}, {"above_10_ohm", 25.0}
            }},
            {"mounting", "chassis; heatsink thermal conditions apply"}
        }},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_DATASHEET"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "RH50 is a series-level record, not a selected resistance SKU. Rated power depends on mounting and thermal conditions; verify derating curve, pulse overload and selected part number."}
        }}
    };


    const auto enamelledCopper = [](const std::string& id, const std::string& grade,
                                    const std::string& enamel, const std::string& baseCoat,
                                    const std::string& topCoat, int thermalClass,
                                    double minDiameterMm, double maxDiameterMm,
                                    const std::string& ansiType) {
        return json{
            {"schemaVersion", 1},
            {"material", {
                {"id", id}, {"manufacturer", "ELEKTRISOLA"},
                {"grade", grade}, {"family", "enameled copper magnet wire"},
                {"conductorMaterial", "Cu-ETP, 99.95% copper"},
                {"resistivityAt20C_ohm_m", 1.71e-8},
                {"temperatureCoefficientAt20C_per_K", 0.00393},
                {"density_kg_per_m3", 8900.0},
                {"conductivity_S_per_m", 58.5e6},
                {"enamelSystem", enamel}, {"baseCoat", baseCoat},
                {"topCoat", topCoat}, {"thermalClass_C", thermalClass},
                {"ansiMWType", ansiType},
                {"availableBareConductorDiameterRange_m",
                    {minDiameterMm * 1.0e-3, maxDiameterMm * 1.0e-3}},
                {"diameterRangeIsCatalogAvailabilityNotSelectedWireSize", true}
            }},
            {"sourceDocuments", json::array({
                "https://www.elektrisola.com/en-us/Products/Enamelled-Wire/Types",
                "https://www.elektrisola.com/Attachments/UL/OBMW2.E331840_-_Magnet_Wire_USA.pdf",
                "https://elektrisoladev.com/Attachments/Metals/ELEKTRISOLA_Datasheet_Cu.pdf"
            })},
            {"dataQuality", {
                {"sourceStatus", "MANUFACTURER_PRODUCT_RANGE_AND_UL_COMPONENT_LISTING"},
                {"resistivityStatus", "MANUFACTURER_COPPER_DATASHEET_TYPICAL"},
                {"wireSizeStatus", "RANGE_ONLY_NO_NOMINAL_DIAMETER_SELECTED"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", "Thermal class is the listed magnet-wire component class, not a complete insulation-system temperature rating. Select an exact IEC 60317 wire size and grade before winding geometry or dielectric design."}
            }}
        };
    };

    const json steelM235 = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "TK-POWERCORE-M235-35A"}, {"manufacturer", "thyssenkrupp Electrical Steel"},
            {"grade", "powercore M235-35A"}, {"family", "non-grain-oriented electrical steel"},
            {"standard", "DIN EN 10106"}, {"laminationThickness_m", 0.00035},
            {"density_kg_per_m3", 7600.0},
            {"maximumSpecificCoreLoss_W_per_kg", {
                {"50Hz_1p0T", 0.95}, {"50Hz_1p5T", 2.35}
            }},
            {"minimumPolarization_T", {
                {"at_2500_A_per_m", 1.49}, {"at_5000_A_per_m", 1.60},
                {"at_10000_A_per_m", 1.70}
            }}
        }},
        {"dataQuality", {
            {"coreLossStatus", "MANUFACTURER_MAXIMUM_VALUES"},
            {"bhCurveStatus", "NOT_PUBLISHED_IN_IMPORTED_DATASET"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Manufacturer table gives maximum specific losses and minimum polarization points. No continuous B-H curve or loss map versus frequency, flux density and temperature is inferred."}
        }}
    };
    const json steelM250 = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "TK-POWERCORE-M250-35A"}, {"manufacturer", "thyssenkrupp Electrical Steel"},
            {"grade", "powercore M250-35A"}, {"family", "non-grain-oriented electrical steel"},
            {"standard", "DIN EN 10106"}, {"laminationThickness_m", 0.00035},
            {"density_kg_per_m3", 7600.0},
            {"maximumSpecificCoreLoss_W_per_kg", {
                {"50Hz_1p0T", 1.05}, {"50Hz_1p5T", 2.50}
            }},
            {"minimumPolarization_T", {
                {"at_2500_A_per_m", 1.49}, {"at_5000_A_per_m", 1.60},
                {"at_10000_A_per_m", 1.70}
            }}
        }},
        {"dataQuality", {
            {"coreLossStatus", "MANUFACTURER_MAXIMUM_VALUES"},
            {"bhCurveStatus", "NOT_PUBLISHED_IN_IMPORTED_DATASET"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Manufacturer table gives maximum specific losses and minimum polarization points. No continuous B-H curve or loss map versus frequency, flux density and temperature is inferred."}
        }}
    };
    const json steelM270 = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "TK-POWERCORE-M270-35A"}, {"manufacturer", "thyssenkrupp Electrical Steel"},
            {"grade", "powercore M270-35A"}, {"family", "non-grain-oriented electrical steel"},
            {"standard", "DIN EN 10106"}, {"laminationThickness_m", 0.00035},
            {"density_kg_per_m3", 7650.0},
            {"maximumSpecificCoreLoss_W_per_kg", {
                {"50Hz_1p0T", 1.10}, {"50Hz_1p5T", 2.70}
            }},
            {"minimumPolarization_T", {
                {"at_2500_A_per_m", 1.49}, {"at_5000_A_per_m", 1.60},
                {"at_10000_A_per_m", 1.70}
            }}
        }},
        {"dataQuality", {
            {"coreLossStatus", "MANUFACTURER_MAXIMUM_VALUES"},
            {"bhCurveStatus", "NOT_PUBLISHED_IN_IMPORTED_DATASET"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Manufacturer table gives maximum specific losses and minimum polarization points. No continuous B-H curve or loss map versus frequency, flux density and temperature is inferred."}
        }}
    };

    const json kaptonHn25 = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "DUPONT-KAPTON-HN-25UM"}, {"manufacturer", "DuPont"},
            {"grade", "Kapton HN, nominal 25 µm"},
            {"family", "polyimide film insulation"},
            {"referenceThickness_m", 25.0e-6},
            {"density_kg_per_m3", 1420.0},
            {"thermalConductivity_W_per_mK", 0.20},
            {"specificHeat_J_per_kgK", 1090.0},
            {"thermalExpansionCoefficient_per_K", 20.0e-6},
            {"dielectricStrength_V_per_m", 303.0e6},
            {"relativePermittivity_1kHz", 3.4},
            {"dissipationFactor_1kHz", 0.0018},
            {"volumeResistivity_ohm_m", 1.5e15},
            {"typicalTensileStrengthAt23C_Pa", 231.0e6}
        }},
        {"sourceDocuments", json::array({
            "https://www.dupont.com/content/dam/dupont/amer/us/en/ei-transformation/public/documents/en/EI-10206-Kapton-HN-Data-Sheet.pdf"
        })},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_DATASHEET_TYPICAL_VALUES"},
            {"dielectricStrengthStatus", "TYPICAL_60HZ_25UM_FILM"},
            {"thermalPropertiesStatus", "TYPICAL_AT_23C"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "The 303 kV/mm dielectric strength is a typical short-term test value for 25 µm film, not an allowable continuous design field. DuPont reports broad application temperature capability; this is not a continuous-use rating for a finished insulation system. Volume resistivity converted from 1.5e17 ohm-cm."}
        }}
    };

    const json tape3m27 = {
        {"schemaVersion", 1},
        {"component", {
            {"id", "3M-SCOTCH-27-GLASS-CLOTH-TAPE"},
            {"manufacturer", "3M"},
            {"grade", "Scotch 27 Glass Cloth Electrical Tape"},
            {"family", "glass cloth electrical insulation tape"},
            {"backingMaterial", "woven glass cloth"},
            {"adhesive", "pressure-sensitive thermosetting rubber"},
            {"backingThickness_m", 0.125e-3},
            {"totalThickness_m", 0.177e-3},
            {"maximumOperatingTemperature_K", 423.15},
            {"typicalDielectricBreakdown_V", 3000.0},
            {"typicalTensileStrength_N_per_m", 25200.0},
            {"typicalElongation_fraction", 0.05},
            {"typicalAdhesionToSteel_N_per_m", 330.0},
            {"ulRecognized", true},
            {"ulTemperature_C", 150.0},
            {"standardWidth_m", 19.0e-3},
            {"standardLength_m", 54.864}
        }},
        {"sourceDocuments", json::array({
            "https://multimedia.3m.com/mws/media/1117261O/3m-scotch-27-glass-cloth-electrical-tape-data-sheet.pdf",
            "https://www.3m.co.uk/3M/en_GB/p/dc/v000532235/"
        })},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_DATASHEET_TYPICAL_VALUES"},
            {"electricalStrengthStatus", "DIELECTRIC_BREAKDOWN_TYPICAL_3000V"},
            {"mechanicalStatus", "TYPICAL_AT_23C"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "3M states typical values are not specification limits. 3000 V is dielectric breakdown voltage for the tape specimen, not a field strength; do not divide by total thickness and treat it as a guaranteed design rating. UL Recognized component listing is stated for 150 °C."}
        }}
    };

    std::vector<SeedRecord> records{
        {"MAT-DUPONT-KAPTON-HN-25UM", "SEED-DUPONT-KAPTON-HN25-R1", "insulation-material",
         "https://www.dupont.com/content/dam/dupont/amer/us/en/ei-transformation/public/documents/en/EI-10206-Kapton-HN-Data-Sheet.pdf",
         "DuPont Kapton HN datasheet EI-10206 (7/24); nominal 25 µm film",
         kaptonHn25},
        {"COMP-3M-SCOTCH-27", "SEED-3M-SCOTCH27-R1", "insulation-material",
         "https://multimedia.3m.com/mws/media/1117261O/3m-scotch-27-glass-cloth-electrical-tape-data-sheet.pdf",
         "3M Scotch 27 manufacturer datasheet; 7 mil glass cloth electrical tape",
         tape3m27},
        {"MAT-TDK-N87", "SEED-TDK-N87-R1", "core-material",
         "https://www.tdk-electronics.tdk.com/download/187238/990c299b916e9f3eb7e44ad563b7f0b9/pdf-n87.pdf",
         "TDK N87 datasheet 2023-02; dynamic curve FAL0599-4 digitized approximately", n87},
        {"MAT-TDK-N97", "SEED-TDK-N97-R1", "core-material",
         "https://www.tdk-electronics.tdk.com/download/528886/cf0d9784d3d2438b52ade72fbb86ecf6/pdf-n97.pdf",
         "TDK N97 datasheet 2023-02; dynamic curve FAL0619-M digitized approximately", n97},
        {"MAT-CU-C11000-ETP", "SEED-CU-C11000-R1", "conductor-material",
         "https://alloys.copper.org/alloy/C11000",
         "Copper.org C11000 reference; conductivity and density; resistivity derived", copper},
        {"MAT-DUPONT-NOMEX-410", "SEED-DUPONT-NOMEX410-R1", "insulation-material",
         "https://www.dupont.com/content/dam/dupont/amer/us/en/safety/public/documents/en/Nomex_410_Tech_Data_Sheet.pdf",
         "DuPont Nomex 410 technical data sheet; 0.25 mm reference thickness", nomex},
        {"COMP-TDK-B32776H1335K000", "SEED-TDK-B32776H1335K000-R1", "capacitor",
         "https://www.tdk-electronics.tdk.com/en/3191480/products/product-catalog/film-capacitors/capacitors-for-dc-link/search-results-deltacap-capacitors",
         "TDK ordering-code catalog listing; B32776H1335K000", tdkCapSmall},
        {"COMP-TDK-B32778G4177K000", "SEED-TDK-B32778G4177K000-R1", "capacitor",
         "https://www.tdk-electronics.tdk.com/en/3191480/products/product-catalog/film-capacitors/capacitors-for-dc-link/search-results-deltacap-capacitors",
         "TDK ordering-code catalog listing; B32778G4177K000", tdkCapLarge},
        {"COMP-VISHAY-RH50", "SEED-VISHAY-RH50-R1", "resistor",
         "https://www.vishay.com/docs/50013/rh.pdf",
         "Vishay RH series datasheet; RH50 series specifications", vishayRh50},

        {"MAT-ELEKTRISOLA-POLYSOL-180", "SEED-ELEKTRISOLA-P180-R1", "conductor-material",
         "https://www.elektrisola.com/en-us/Products/Enamelled-Wire/Types",
         "ELEKTRISOLA product type table and UL component listing; Polysol 180, MW 82-C, thermal class 180", 
         enamelledCopper("ELEKTRISOLA-POLYSOL-180", "Polysol 180", "modified polyurethane", "modified polyurethane", "none", 180, 0.010, 0.500, "MW 82-C")},
        {"MAT-ELEKTRISOLA-ESTERSOL-180", "SEED-ELEKTRISOLA-E180-R1", "conductor-material",
         "https://www.elektrisola.com/en-us/Products/Enamelled-Wire/Types",
         "ELEKTRISOLA product type table and UL component listing; Estersol 180, MW 77-C, thermal class 180",
         enamelledCopper("ELEKTRISOLA-ESTERSOL-180", "Estersol 180", "solderable polyesterimide", "solderable polyesterimide", "none", 180, 0.010, 0.500, "MW 77-C")},
        {"MAT-ELEKTRISOLA-AMIDESTER-200", "SEED-ELEKTRISOLA-A200-R1", "conductor-material",
         "https://www.elektrisola.com/en-us/Products/Enamelled-Wire/Types",
         "ELEKTRISOLA product type table and UL component listing; Amidester 200, MW 74-C, thermal class 200",
         enamelledCopper("ELEKTRISOLA-AMIDESTER-200", "Amidester 200", "THEIC-modified polyesterimide", "THEIC-modified polyesterimide", "none", 200, 0.010, 0.500, "MW 74-C")},
        {"MAT-ELEKTRISOLA-AMIDESTER-AI210", "SEED-ELEKTRISOLA-AI210-R1", "conductor-material",
         "https://www.elektrisola.com/en-us/Products/Enamelled-Wire/Types",
         "ELEKTRISOLA product type table and UL component listing; Amidester AI210, MW 35-C, UL thermal class 200",
         enamelledCopper("ELEKTRISOLA-AMIDESTER-AI210", "Amidester AI210", "polyesterimide plus polyamideimide overcoat", "polyesterimide", "polyamideimide", 200, 0.015, 0.500, "MW 35-C")},
        {"MAT-ELEKTRISOLA-AMIDIMIDE-220", "SEED-ELEKTRISOLA-I220-R1", "conductor-material",
         "https://www.elektrisola.com/en-us/Products/Enamelled-Wire/Types",
         "ELEKTRISOLA product type table and UL component listing; Amidimide 220, MW 81-C, thermal class 220",
         enamelledCopper("ELEKTRISOLA-AMIDIMIDE-220", "Amidimide 220", "polyamideimide", "polyamideimide", "none", 220, 0.020, 0.500, "MW 81-C")},
        {"MAT-ELEKTRISOLA-POLYIMIDE-240", "SEED-ELEKTRISOLA-ML240-R1", "conductor-material",
         "https://www.elektrisola.com/en-us/Products/Enamelled-Wire/Types",
         "ELEKTRISOLA product type table and UL component listing; Polyimide 240, MW 16-C, thermal class 240",
         enamelledCopper("ELEKTRISOLA-POLYIMIDE-240", "Polyimide 240 (ML 240)", "aromatic polyimide", "aromatic polyimide", "none", 240, 0.015, 0.500, "MW 16-C")},
        {"MAT-TK-POWERCORE-M235-35A", "SEED-TK-M235-35A-R1", "core-material",
         "https://www.thyssenkrupp-steel.com/en/products/electrical-steel/electrical-steel-non-grain-oriented/powercore-a/powercore-a.html",
         "thyssenkrupp powercore A product table; M235-35A, DIN EN 10106",
         steelM235},
        {"MAT-TK-POWERCORE-M250-35A", "SEED-TK-M250-35A-R1", "core-material",
         "https://www.thyssenkrupp-steel.com/en/products/electrical-steel/electrical-steel-non-grain-oriented/powercore-a/powercore-a.html",
         "thyssenkrupp powercore A product table; M250-35A, DIN EN 10106",
         steelM250},
        {"MAT-TK-POWERCORE-M270-35A", "SEED-TK-M270-35A-R1", "core-material",
         "https://www.thyssenkrupp-steel.com/en/products/electrical-steel/electrical-steel-non-grain-oriented/powercore-a/powercore-a.html",
         "thyssenkrupp powercore A product table; M270-35A, DIN EN 10106",
         steelM270},
    };

    // Exact TDK ordering codes and AL data transcribed from the manufacturer's
    // PQ core search table. Geometry dimensions are intentionally not inferred.
    const std::vector<std::tuple<std::string, std::string, std::string, double>> tdkPqCores{
        {"B65877B0000R049", "PQ 26/20", "N49", 3850},
        {"B65883A0000R049", "PQ 40/40", "N49", 3200},
        {"B65875A0000R095", "PQ 20/20", "N95", 3300},
        {"B65879B0000R049", "PQ 32/30", "N49", 3450},
        {"B65879B0000R087", "PQ 32/30", "N87", 4800},
        {"B65875A0000R097", "PQ 20/20", "N97", 2750},
        {"B65883B0000R097", "PQ 40/30", "N97", 5300},
        {"B65883A0000R087", "PQ 40/40", "N87", 4300},
        {"B65883A0000R092", "PQ 40/40", "N92", 3200},
        {"B65879B0000R097", "PQ 32/30", "N97", 5000},
        {"B65877B0000R087", "PQ 26/20", "N87", 5000},
        {"B65879B0000R095", "PQ 32/30", "N95", 6100},
        {"B65883B0000R049", "PQ 40/30", "N49", 3900},
        {"B65879A0000R095", "PQ 32/20", "N95", 7600},
        {"B65875B0000R097", "PQ 20/16", "N97", 3200},
        {"B65883B0000R087", "PQ 40/30", "N87", 5200},
        {"B65879A0000R049", "PQ 32/20", "N49", 4600},
        {"B65875B0000R092", "PQ 20/16", "N92", 2400},
        {"B65875B0000R095", "PQ 20/16", "N95", 3750},
        {"B65879A0000R087", "PQ 32/20", "N87", 6300},
        {"B65883B0000R092", "PQ 40/30", "N92", 3900},
        {"B65875A0000R049", "PQ 20/20", "N49", 2000},
        {"B65883B0000R095", "PQ 40/30", "N95", 6500},
        {"B65875A0000R087", "PQ 20/20", "N87", 2650},
        {"B65879A0000R097", "PQ 32/20", "N97", 6500},
        {"B65877A0000R097", "PQ 26/25", "N97", 4650},
        {"B65981B0000R097", "PQ 50/40", "N97", 7800},
        {"B65881A0000R087", "PQ 35/35", "N87", 4500},
        {"B65875B0000R049", "PQ 20/16", "N49", 2400},
        {"B65881A0000R092", "PQ 35/35", "N92", 3300},
        {"B65885A0000R095", "PQ 16/11.6", "N95", 2750},
        {"B65877A0000R087", "PQ 26/25", "N87", 4500},
        {"B65981B0000R092", "PQ 50/40", "N92", 5500},
        {"B65877A0000R095", "PQ 26/25", "N95", 5700},
        {"B65885A0000R097", "PQ 16/11.6", "N97", 2450},
        {"B65981B0000R095", "PQ 50/40", "N95", 9400},
        {"B65875B0000R087", "PQ 20/16", "N87", 3100},
        {"B65881A0000R095", "PQ 35/35", "N95", 5700},
        {"B65881A0000R097", "PQ 35/35", "N97", 4700},
        {"B65877B0000R097", "PQ 26/20", "N97", 5150},
        {"B65885A0000R049", "PQ 16/11.6", "N49", 1900},
        {"B65883A0000R095", "PQ 40/40", "N95", 5500},
        {"B65877B0000R092", "PQ 26/20", "N92", 3850},
        {"B65883A0000R097", "PQ 40/40", "N97", 4500},
        {"B65881A0000R049", "PQ 35/35", "N49", 3300},
        {"B65877B0000R095", "PQ 26/20", "N95", 6300},
        {"B65981B0000R087", "PQ 50/40", "N87", 7600},
        {"B65885A0000R087", "PQ 16/11.6", "N87", 2350},
        {"B65885A0000R092", "PQ 16/11.6", "N92", 1900},
        {"B65877A0000R049", "PQ 26/25", "N49", 3300}
    };

    const std::string tdkPqUrl =
        "https://www.tdk-electronics.tdk.com/en/529374/products/product-catalog/ferrites-and-accessories/pq-cores-and-accessories/search-results-pq-cores-and-accessories";
    for (const auto& [orderingCode, shape, material, alValue] : tdkPqCores) {
        const std::string id = "COMP-TDK-" + orderingCode;
        const json payload = {
            {"schemaVersion", 1},
            {"component", {
                {"id", id}, {"manufacturer", "TDK Electronics"},
                {"orderingCode", orderingCode}, {"coreShape", shape},
                {"ferriteMaterial", material}, {"coreType", "PQ ferrite core"},
                {"inductanceFactor_AL_nH_per_turn_squared", alValue},
                {"AL_tolerance_fraction", { -0.20, 0.30 }},
                {"airGap", "ungapped"},
                {"turnsSquaredForInductance", "L = AL * N^2"},
                {"dimensionsImported", false}
            }},
            {"sourceDocuments", json::array({tdkPqUrl})},
            {"dataQuality", {
                {"sourceStatus", "MANUFACTURER_LIVE_PRODUCT_SEARCH_TABLE"},
                {"alValueStatus", "MANUFACTURER_LISTED_NOMINAL_AL"},
                {"toleranceStatus", "MANUFACTURER_LISTED_PLUS_30_MINUS_20_PERCENT"},
                {"geometryStatus", "DIMENSIONS_NOT_IMPORTED"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", "Exact TDK ordering code, PQ geometry, ferrite material and AL value are imported from the manufacturer search result. AL tolerance is +30/-20 percent. Mechanical dimensions, effective area, effective path length, volume and loss curves must be loaded from the individual datasheet before CAD/FEM or final magnetic design."}
            }}
        };
        records.push_back(SeedRecord{
            id, "SEED-" + orderingCode + "-R1", "core-component",
            tdkPqUrl, "TDK PQ core catalog search; ordering code " + orderingCode,
            payload
        });
    }

    // Manufacturer-listed fully finished non-grain-oriented powercore A grades.
    // Values are maximum specific losses and minimum polarization from the
    // thyssenkrupp product-range table; no B-H curve is synthesized.
    const std::vector<std::tuple<std::string, double, double, double, double,
                                 double, double, double>> tkElectricalSteels{
        {"M235-35A", 0.35, 7.6, 2.35, 0.95, 1.49, 1.6, 1.7},
        {"M250-35A", 0.35, 7.6, 2.5, 1.05, 1.49, 1.6, 1.7},
        {"M270-35A", 0.35, 7.65, 2.7, 1.1, 1.49, 1.6, 1.7},
        {"M300-35A", 0.35, 7.65, 3, 1.2, 1.49, 1.6, 1.7},
        {"M330-35A", 0.35, 7.65, 3.3, 1.3, 1.49, 1.6, 1.7},
        {"M250-50A", 0.5, 7.6, 2.5, 1.05, 1.49, 1.6, 1.7},
        {"M270-50A", 0.5, 7.6, 2.7, 1.1, 1.49, 1.6, 1.7},
        {"M290-50A", 0.5, 7.6, 2.9, 1.15, 1.49, 1.6, 1.7},
        {"M310-50A", 0.5, 7.65, 3.1, 1.25, 1.49, 1.6, 1.7},
        {"M330-50A", 0.5, 7.65, 3.3, 1.35, 1.49, 1.6, 1.7},
        {"M350-50A", 0.5, 7.65, 3.5, 1.5, 1.5, 1.6, 1.7},
        {"M400-50A", 0.5, 7.7, 4, 1.7, 1.53, 1.63, 1.73},
        {"M470-50A", 0.5, 7.7, 4.7, 2, 1.54, 1.64, 1.74},
        {"M530-50A", 0.5, 7.7, 5.3, 2.3, 1.56, 1.65, 1.75},
        {"M600-50A", 0.5, 7.75, 6, 2.6, 1.57, 1.66, 1.76},
        {"M700-50A", 0.5, 7.8, 7, 3, 1.6, 1.69, 1.77},
        {"M800-50A", 0.5, 7.8, 8, 3.6, 1.6, 1.7, 1.78},
        {"M940-50A", 0.5, 7.85, 9.4, 4.2, 1.62, 1.72, 1.81},
        {"M310-65A", 0.65, 7.6, 3.1, 1.25, 1.49, 1.6, 1.7},
        {"M330-65A", 0.65, 7.6, 3.3, 1.35, 1.49, 1.6, 1.7},
        {"M350-65A", 0.65, 7.6, 3.5, 1.5, 1.49, 1.6, 1.7},
        {"M400-65A", 0.65, 7.65, 4, 1.7, 1.52, 1.62, 1.72},
        {"M470-65A", 0.65, 7.65, 4.7, 2, 1.53, 1.63, 1.73},
        {"M530-65A", 0.65, 7.7, 5.3, 2.3, 1.54, 1.64, 1.74},
        {"M600-65A", 0.65, 7.75, 6, 2.6, 1.56, 1.66, 1.76},
        {"M700-65A", 0.65, 7.75, 7, 3, 1.57, 1.67, 1.76},
        {"M800-65A", 0.65, 7.8, 8, 3.6, 1.6, 1.7, 1.78},
        {"M1000-65A", 0.65, 7.8, 10, 4.4, 1.61, 1.71, 1.8},
        {"M600-100A", 1, 7.6, 6, 2.6, 1.53, 1.63, 1.72},
        {"M700-100A", 1, 7.65, 7, 3, 1.54, 1.64, 1.73},
        {"M800-100A", 1, 7.7, 8, 3.6, 1.56, 1.66, 1.75},
        {"940-100A", 1, 7.8, 9.4, 4.2, 1.58, 1.68, 1.78}
    };
    const std::string tkSteelUrl =
        "https://www.thyssenkrupp-steel.com/media/content_1/publikationen/lieferprogramme/thyssenkrupp_product-range_no-electrical-steel_powercore_steel_en.pdf";
    for (const auto& [grade, thicknessMm, density, loss15, loss10, j2500, j5000, j10000] : tkElectricalSteels) {
        const std::string id = "MAT-TK-POWERCORE-" + grade;
        const json payload = {
            {"schemaVersion", 1},
            {"material", {
                {"id", id}, {"manufacturer", "thyssenkrupp Steel"},
                {"grade", "powercore " + grade},
                {"family", "fully finished non-grain-oriented electrical steel"},
                {"standard", grade == "940-100A" ? "Special mill grade" : "DIN EN 10106"},
                {"laminationThickness_m", thicknessMm * 1.0e-3},
                {"density_kg_per_m3", density * 1000.0},
                {"maximumSpecificCoreLoss_W_per_kg", {
                    {"50Hz_1p0T", loss10}, {"50Hz_1p5T", loss15}
                }},
                {"minimumPolarization_T", {
                    {"at_2500_A_per_m", j2500}, {"at_5000_A_per_m", j5000},
                    {"at_10000_A_per_m", j10000}
                }}
            }},
            {"sourceDocuments", json::array({tkSteelUrl})},
            {"dataQuality", {
                {"sourceStatus", "MANUFACTURER_PRODUCT_RANGE_TABLE"},
                {"coreLossStatus", "MANUFACTURER_MAXIMUM_VALUES_AT_50HZ"},
                {"polarizationStatus", "MANUFACTURER_MINIMUM_VALUES"},
                {"bhCurveStatus", "NOT_PUBLISHED_IN_IMPORTED_DATASET"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", "Manufacturer maximum specific losses are tabulated at 50 Hz and stated flux density. Minimum polarization is supplied only at the listed field strengths. These points are not interpolated into a fabricated B-H curve; punching, stacking factor, coating and processing effects remain application-specific."}
            }}
        };
        records.push_back(SeedRecord{
            id, "SEED-" + id + "-R1", "core-material", tkSteelUrl,
            "thyssenkrupp powercore A product range; " + grade, payload
        });
    }

    // Manufacturer-listed grain-oriented powercore grades for transformer cores.
    // Losses are maximum values at 1.7 T / 50 Hz; do not infer a full loss map.
    const std::vector<std::tuple<std::string, std::string, double, double, double>> tkGrainOriented{
        {"H070-20", "M70-20R5", 0.20, 0.70, 1.88},
        {"H075-20", "M75-20R5", 0.20, 0.75, 1.88},
        {"H070-23", "", 0.23, 0.70, 1.88},
        {"H075-23", "M75-23R5", 0.23, 0.75, 1.88},
        {"H078-23", "", 0.23, 0.78, 1.88},
        {"H080-23", "M80-23R5", 0.23, 0.80, 1.88},
        {"H085-23", "M85-23R5", 0.23, 0.85, 1.88},
        {"H090-23", "M90-23R5", 0.23, 0.90, 1.88},
        {"H100-23", "M100-23P5", 0.23, 1.00, 1.85},
        {"H085-27", "M85-27R5", 0.27, 0.85, 1.88},
        {"H090-27", "M90-27R5", 0.27, 0.90, 1.88},
        {"H095-27", "", 0.27, 0.95, 1.88},
        {"H100-27", "", 0.27, 1.00, 1.88},
        {"H100-30", "M100-30P5", 0.30, 1.00, 1.88},
        {"H105-30", "M105-30P5", 0.30, 1.05, 1.88},
        {"H110-30", "M110-30P5", 0.30, 1.10, 1.88},
        {"H125-35", "M125-35P5", 0.35, 1.25, 1.88}
    };
    const std::string tkGoSteelUrl =
        "https://www.thyssenkrupp-steel.com/en/products/electrical-steel/electrical-steel-grain-oriented/powercore/powercore.html";
    for (const auto& [grade, iecGrade, thicknessMm, loss17, polarization800] : tkGrainOriented) {
        const std::string id = "MAT-TK-POWERCORE-" + grade;
        const json payload = {
            {"schemaVersion", 1},
            {"material", {
                {"id", id}, {"manufacturer", "thyssenkrupp Electrical Steel"},
                {"grade", "powercore " + grade},
                {"iec60404_8_7Grade", iecGrade.empty() ? json(nullptr) : json(iecGrade)},
                {"family", "grain-oriented electrical steel"},
                {"standard", "EN 10107 / IEC 60404-8-7"},
                {"laminationThickness_m", thicknessMm * 1.0e-3},
                {"maximumSpecificCoreLoss_W_per_kg", {{"50Hz_1p7T", loss17}}},
                {"minimumPolarization_T", {{"at_800_A_per_m", polarization800}}}
            }},
            {"sourceDocuments", json::array({tkGoSteelUrl,
                "https://www.thyssenkrupp-steel.com/media/content_1/publikationen/epd/thyssenkrupp_epd_grain_oriented_electrical_steel_en_steel.pdf"})},
            {"dataQuality", {
                {"sourceStatus", "MANUFACTURER_PRODUCT_RANGE_AND_EPD"},
                {"coreLossStatus", "MANUFACTURER_MAXIMUM_VALUE_AT_50HZ_1P7T"},
                {"polarizationStatus", "MANUFACTURER_MINIMUM_AT_800A_PER_M"},
                {"bhCurveStatus", "NOT_PUBLISHED_IN_IMPORTED_DATASET"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", "Grain-oriented steel is anisotropic: magnetic loss and polarization apply along the rolling direction. Do not use these values for transverse flux or synthesize a B-H curve. Confirm grade, coating, stress-relief annealing and laser-domain treatment with the ordered coil certificate."}
            }}
        };
        records.push_back(SeedRecord{
            id, "SEED-" + id + "-R1", "core-material", tkGoSteelUrl,
            "thyssenkrupp powercore grain-oriented product range; " + grade, payload
        });
    }

    // Exact TDK ETD ordering codes, AL values and air gaps from manufacturer
    // ER/ETD/EQ search tables. Gapped AL tolerance is left unspecified where
    // the catalog does not publish one.
    const std::vector<std::tuple<std::string, std::string, std::string, double, double>> tdkEtdCores{
        {"B66358G0500X187", "ETD 29/16/10", "N87", 201, 0.50},
        {"B66358G0500X127", "ETD 29/16/10", "N27", 201, 0.50},
        {"B66358G0200X187", "ETD 29/16/10", "N87", 383, 0.20},
        {"B66358G0200X127", "ETD 29/16/10", "N27", 383, 0.20},
        {"B66358G0100X187", "ETD 29/16/10", "N87", 621, 0.10},
        {"B66358G1000X187", "ETD 29/16/10", "N87", 124, 1.00},
        {"B66358G1000X127", "ETD 29/16/10", "N27", 124, 1.00},
        {"B66358G0100X127", "ETD 29/16/10", "N27", 621, 0.10},
        {"B66358G0000X197", "ETD 29/16/10", "N97", 2250, 0.0},
        {"B66358G0000X187", "ETD 29/16/10", "N87", 2200, 0.0},
        {"B66358G0000X127", "ETD 29/16/10", "N27", 2000, 0.0},
        {"B66363G2000X187", "ETD 39/20/13", "N87", 115, 2.00},
        {"B66363G1000X187", "ETD 39/20/13", "N87", 196, 1.00},
        {"B66363G1000X127", "ETD 39/20/13", "N27", 196, 1.00},
        {"B66363G0500X187", "ETD 39/20/13", "N87", 326, 0.50},
        {"B66363G0500X127", "ETD 39/20/13", "N27", 326, 0.50},
        {"B66363G2000X127", "ETD 39/20/13", "N27", 115, 2.00},
        {"B66363G0000X187", "ETD 39/20/13", "N87", 2700, 0.0},
        {"B66363G0000X127", "ETD 39/20/13", "N27", 2550, 0.0},
        {"B66363G0200X127", "ETD 39/20/13", "N27", 639, 0.20},
        {"B66363G0100X187", "ETD 39/20/13", "N87", 1062, 0.10},
        {"B66363G0100X127", "ETD 39/20/13", "N27", 1062, 0.10},
        {"B66363G0000X197", "ETD 39/20/13", "N97", 2800, 0.0},
        {"B66363G0200X187", "ETD 39/20/13", "N87", 639, 0.20}
    };
    const std::string tdkEtdUrl =
        "https://www.tdk-electronics.tdk.com/en/529378/products/product-catalog/ferrites-and-accessories/er-etd-eq-cores-and-accessories-/search-results-er-etd-eq-cores-and-accessories";
    for (const auto& [orderingCode, shape, material, alValue, airGapMm] : tdkEtdCores) {
        const std::string id = "COMP-TDK-" + orderingCode;
        const bool ungapped = airGapMm == 0.0;
        const json payload = {
            {"schemaVersion", 1},
            {"component", {
                {"id", id}, {"manufacturer", "TDK Electronics"},
                {"orderingCode", orderingCode}, {"coreShape", shape},
                {"ferriteMaterial", material}, {"coreType", "ETD ferrite core"},
                {"inductanceFactor_AL_nH_per_turn_squared", alValue},
                {"airGap", ungapped ? json("ungapped") : json(airGapMm)},
                {"AL_tolerance", ungapped ? json("+30/-20 percent") : json("not specified in source table")},
                {"dimensionsImported", false}
            }},
            {"sourceDocuments", json::array({tdkEtdUrl})},
            {"dataQuality", {
                {"sourceStatus", "MANUFACTURER_LIVE_PRODUCT_SEARCH_TABLE"},
                {"alValueStatus", "MANUFACTURER_LISTED_NOMINAL_AL"},
                {"airGapStatus", "MANUFACTURER_LISTED"},
                {"geometryStatus", "DIMENSIONS_NOT_IMPORTED"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", "Exact ordering code, core shape, material, AL and air gap are manufacturer-listed. For gapped variants the search table does not state AL tolerance. Import dimensions, Ae, le, Ve and loss curves from the individual datasheet before CAD/FEM or release."}
            }}
        };
        records.push_back(SeedRecord{
            id, "SEED-" + orderingCode + "-R1", "core-component",
            tdkEtdUrl, "TDK ER/ETD/EQ catalog search; " + orderingCode, payload
        });
    }

    // Electrical conductor stock materials. Values at 20 C; geometry templates
    // below are parametric and do not imply a fixed commercial size.
    const auto conductorMaterial = [](const std::string& id, const std::string& manufacturer,
                                      const std::string& grade, const std::string& family,
                                      double resistivity, double density, double alpha,
                                      double iacs, const std::string& source,
                                      const std::string& propertyStatus) {
        json material = {
            {"id", id}, {"manufacturer", manufacturer}, {"grade", grade},
            {"family", family}, {"resistivityAt20C_ohm_m", resistivity},
            {"density_kg_per_m3", density},
            {"conductivity_percent_IACS", iacs},
            {"referenceTemperature_K", 293.15}
        };
        if (alpha > 0.0)
            material["temperatureCoefficientAt20C_per_K"] = alpha;
        const json payload = {
            {"schemaVersion", 1}, {"material", material},
            {"sourceDocuments", json::array({source})},
            {"dataQuality", {
                {"sourceStatus", propertyStatus},
                {"electricalPropertiesStatus", "MANUFACTURER_OR_STANDARD_PUBLISHED_AT_20C"},
                {"temperatureCoefficientStatus", alpha > 0.0 ? "REFERENCE_VALUE" : "NOT_PUBLISHED_FOR_COMPOSITE"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", "Reference properties at 20 C. Actual values depend on temper, product form, processing and batch certification. Use exact supplier certificate and applicable product standard for released design."}
            }}
        };
        return payload;
    };
    const std::string aluminiumSource =
        "https://inotal.hu/en/products/wire-rod/continuous-cast-ec-grade-wire-rod/";
    const std::string ccaSource =
        "https://www.alleima.com/en/products/wire/ultra-fine-medical-wire/wire-products/fine-wire/conductive-wire/copper-clad-aluminium/";
    const std::string copperSource =
        "https://www.avivametals.com/assets/files/aviva-c11000.pdf";
    const std::string copperOfhcSource =
        "https://www.alleima.com/en/products/wire/ultra-fine-medical-wire/wire-products/fine-wire/conductive-wire/copper/";
    records.push_back({"MAT-CU-C10100", "SEED-CU-C10100-R1", "conductor-material",
        copperOfhcSource, "Copper C10100 oxygen-free high conductivity reference",
        conductorMaterial("CU-C10100", "Copper Development Association / UNS",
            "C10100 OFHC copper", "oxygen-free high conductivity copper",
            1.72e-8, 8940.0, 0.00397, 100.0, copperOfhcSource, "MANUFACTURER_DATASHEET")});
    records.push_back({"MAT-CU-C10200", "SEED-CU-C10200-R1", "conductor-material",
        copperOfhcSource, "Copper C10200 oxygen-free copper reference",
        conductorMaterial("CU-C10200", "UNS C10200", "C10200 OF copper",
            "oxygen-free copper", 1.72e-8, 8940.0, 0.00397, 100.0,
            copperOfhcSource, "MANUFACTURER_DATASHEET")});
    records.push_back({"MAT-CU-C11000", "SEED-CU-C11000-R1", "conductor-material",
        copperSource, "Copper C11000 ETP copper reference",
        conductorMaterial("CU-C11000", "UNS C11000", "C11000 ETP copper",
            "electrolytic tough pitch copper", 1.724e-8, 8910.0, 0.00393, 100.0,
            copperSource, "MANUFACTURER_PRODUCT_DATASHEET")});
    records.push_back({"MAT-AL-1350-H0", "SEED-AL-1350-H0-R1", "conductor-material",
        aluminiumSource, "EN AW-1350 / EC grade aluminium H0",
        conductorMaterial("AL-1350-H0", "Inotal Aluminium", "EN AW-1350 H0",
            "electrical conductor aluminium", 2.755e-8, 2703.0, 0.00403, 63.3,
            aluminiumSource, "EN_1715_2_TABLE")});
    records.push_back({"MAT-AL-1370-H0", "SEED-AL-1370-H0-R1", "conductor-material",
        aluminiumSource, "EN AW-1370 / EC grade aluminium H0",
        conductorMaterial("AL-1370-H0", "Inotal Aluminium", "EN AW-1370 H0",
            "electrical conductor aluminium", 2.725e-8, 2703.0, 0.00403, 63.3,
            aluminiumSource, "EN_1715_2_TABLE")});
    records.push_back({"MAT-AL-6101-T4", "SEED-AL-6101-T4-R1", "conductor-material",
        aluminiumSource, "EN AW-6101 T4 aluminium alloy",
        conductorMaterial("AL-6101-T4", "Inotal Aluminium", "EN AW-6101 T4",
            "Al-Mg-Si electrical conductor alloy", 3.50e-8, 2700.0, 0.00403, 49.2,
            aluminiumSource, "MANUFACTURER_PRODUCT_TABLE")});
    records.push_back({"MAT-CCA-10", "SEED-CCA-10-R1", "conductor-material",
        ccaSource, "Copper-clad aluminium 10 percent copper by volume",
        conductorMaterial("CCA-10", "Kanthal / Alleima", "10% CCA",
            "copper-clad aluminium composite conductor", 2.744e-8, 3320.0, 0.0,
            62.854, ccaSource, "MANUFACTURER_DATASHEET")});
    records.push_back({"MAT-CCA-15", "SEED-CCA-15-R1", "conductor-material",
        ccaSource, "Copper-clad aluminium 15 percent copper by volume",
        conductorMaterial("CCA-15", "Kanthal / Alleima", "15% CCA",
            "copper-clad aluminium composite conductor", 2.678e-8, 3630.0, 0.0,
            64.416, ccaSource, "MANUFACTURER_DATASHEET")});

    const auto conductorProfile = [](const std::string& id, const std::string& grade,
                                     const std::string& form, const std::string& geometry,
                                     const std::string& applicableStandard,
                                     const std::string& source, const std::string& note) {
        return json{
            {"schemaVersion", 1},
            {"component", {
                {"id", id}, {"materialGrade", grade}, {"productForm", form},
                {"crossSectionGeometry", geometry},
                {"dimensions", "PARAMETRIC_USER_INPUT"},
                {"dimensionUnits", "m"},
                {"applicableStandard", applicableStandard},
                {"dimensionsAreManufacturerStockSizes", false}
            }},
            {"sourceDocuments", json::array({source})},
            {"dataQuality", {
                {"sourceStatus", "GEOMETRY_TEMPLATE_WITH_REFERENCED_PRODUCT_FORM"},
                {"dimensionStatus", "NO_FIXED_DIMENSIONS_ASSUMED"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", note}
            }}
        };
    };
    const std::string copperFormsNote =
        "Parametric cross-section template only. Select width, thickness, diameter, wall thickness and temper from a supplier drawing and applicable product standard. Do not infer ampacity without installation, cooling and AC-loss analysis.";
    const std::string aluminiumFormsNote =
        "Parametric aluminium conductor template. Confirm alloy, temper, dimensions, conductivity and mechanical properties from supplier certificate. Aluminium joint oxidation and connection design require separate treatment.";
    const std::string ccaFormsNote =
        "Round CCA wire form only; copper fraction and conductor properties are manufacturer-listed. Confirm diameter, copper layer uniformity, temper and insulation from the selected product datasheet.";
    const std::vector<std::tuple<std::string, std::string, std::string, std::string,
                                 std::string, std::string>> conductorProfiles{
        {"CU-C10100", "round-wire", "circle", "EN 13601 / ASTM B49", copperSource, copperFormsNote},
        {"CU-C10100", "rectangular-wire", "rectangle", "IEC 60317 / supplier drawing", copperSource, copperFormsNote},
        {"CU-C10100", "strip", "rectangle", "EN 13599 / ASTM B152", copperSource, copperFormsNote},
        {"CU-C10100", "flat-bar-busbar", "rectangle", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C10100", "round-bar", "circle", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C10100", "square-bar", "square", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C10100", "hex-bar", "regular-hexagon", "Supplier drawing", copperSource, copperFormsNote},
        {"CU-C10100", "tube", "hollow-circle", "Supplier drawing", copperSource, copperFormsNote},
        {"CU-C10200", "round-wire", "circle", "EN 13601 / ASTM B49", copperSource, copperFormsNote},
        {"CU-C10200", "rectangular-wire", "rectangle", "IEC 60317 / supplier drawing", copperSource, copperFormsNote},
        {"CU-C10200", "strip", "rectangle", "EN 13599 / ASTM B152", copperSource, copperFormsNote},
        {"CU-C10200", "flat-bar-busbar", "rectangle", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C10200", "round-bar", "circle", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C10200", "square-bar", "square", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C10200", "hex-bar", "regular-hexagon", "Supplier drawing", copperSource, copperFormsNote},
        {"CU-C10200", "tube", "hollow-circle", "Supplier drawing", copperSource, copperFormsNote},
        {"CU-C11000", "round-wire", "circle", "EN 13601 / ASTM B49", copperSource, copperFormsNote},
        {"CU-C11000", "rectangular-wire", "rectangle", "IEC 60317 / supplier drawing", copperSource, copperFormsNote},
        {"CU-C11000", "strip", "rectangle", "EN 13599 / ASTM B152", copperSource, copperFormsNote},
        {"CU-C11000", "flat-bar-busbar", "rectangle", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C11000", "round-bar", "circle", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C11000", "square-bar", "square", "EN 13601 / ASTM B187", copperSource, copperFormsNote},
        {"CU-C11000", "hex-bar", "regular-hexagon", "Supplier drawing", copperSource, copperFormsNote},
        {"CU-C11000", "tube", "hollow-circle", "Supplier drawing", copperSource, copperFormsNote},
        {"AL-1350-H0", "round-wire-rod", "circle", "EN 1715-2 / EN 573-3", aluminiumSource, aluminiumFormsNote},
        {"AL-1350-H0", "rectangular-wire", "rectangle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-1350-H0", "strip", "rectangle", "EN 1715-2 / supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-1350-H0", "flat-bar-busbar", "rectangle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-1350-H0", "round-bar", "circle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-1370-H0", "round-wire-rod", "circle", "EN 1715-2 / EN 573-3", aluminiumSource, aluminiumFormsNote},
        {"AL-1370-H0", "rectangular-wire", "rectangle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-1370-H0", "strip", "rectangle", "EN 1715-2 / supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-1370-H0", "flat-bar-busbar", "rectangle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-1370-H0", "round-bar", "circle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-6101-T4", "round-wire-rod", "circle", "EN 1715-2 / EN 573-3", aluminiumSource, aluminiumFormsNote},
        {"AL-6101-T4", "strip", "rectangle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-6101-T4", "flat-bar-busbar", "rectangle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"AL-6101-T4", "round-bar", "circle", "Supplier drawing", aluminiumSource, aluminiumFormsNote},
        {"CCA-10", "round-wire", "circle", "Manufacturer product datasheet", ccaSource, ccaFormsNote},
        {"CCA-15", "round-wire", "circle", "Manufacturer product datasheet", ccaSource, ccaFormsNote},
        {"CU-C10100", "foil", "rectangle", "Supplier drawing / ASTM B152", copperOfhcSource, copperFormsNote},
        {"CU-C10200", "foil", "rectangle", "Supplier drawing / ASTM B152", copperOfhcSource, copperFormsNote},
        {"CU-C11000", "foil", "rectangle", "ASTM B152", copperSource, copperFormsNote},
        {"AL-1350-H0", "foil", "rectangle", "Supplier drawing / EN 485", aluminiumSource, aluminiumFormsNote},
        {"AL-1370-H0", "foil", "rectangle", "Supplier drawing / EN 485", aluminiumSource, aluminiumFormsNote},
        {"CU-C11000", "hollow-rectangular-water-cooled", "hollow-rectangle", "Supplier drawing", copperSource, copperFormsNote}
    };
    for (const auto& [grade, form, geometry, standard, source, note] : conductorProfiles) {
        const std::string id = "FORM-" + grade + "-" + form;
        const json payload = conductorProfile(id, grade, form, geometry, standard, source, note);
        records.push_back(SeedRecord{
            id, "SEED-" + id + "-R1", "conductor-stock-profile",
            source, "Parametric conductor stock profile " + grade + " " + form,
            payload
        });
    }
    // Flexible insulation products with thickness-specific manufacturer data.
    // Short-term dielectric strength is never treated as a continuous design stress.
    const auto addInsulationFilm = [&records](
        const std::string& id, const std::string& manufacturer,
        const std::string& grade, const std::string& family,
        const std::string& source, double thicknessM,
        double shortTermDielectricStrengthVPerM,
        const std::string& dielectricTest, const std::string& thermalClass,
        const std::string& sourceStatus, const std::string& notes) {
        const json payload = {
            {"schemaVersion", 1},
            {"material", {
                {"id", id}, {"manufacturer", manufacturer}, {"grade", grade},
                {"family", family}, {"nominalThickness_m", thicknessM},
                {"typicalShortTermDielectricStrength_V_per_m", shortTermDielectricStrengthVPerM},
                {"dielectricTestMethod", dielectricTest},
                {"thermalClass", thermalClass.empty() ? json(nullptr) : json(thermalClass)}
            }},
            {"sourceDocuments", json::array({source})},
            {"dataQuality", {
                {"sourceStatus", sourceStatus},
                {"dielectricStrengthStatus", "TYPICAL_SHORT_TERM_MANUFACTURER_TEST_VALUE"},
                {"continuousDesignStressStatus", "NOT_PUBLISHED_DO_NOT_INFER"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", notes}
            }}
        };
        records.push_back(SeedRecord{
            id, "SEED-" + id + "-R1", "insulation-material",
            source, manufacturer + " " + grade + " " + std::to_string(thicknessM * 1.0e6) + " um",
            payload
        });
    };
    const std::string nomex410Source =
        "https://www.dupont.com/content/dam/dupont/amer/us/en/safety/public/documents/en/Nomex_410_Datasheet_2025.pdf";
    const std::string kaptonHnSource =
        "https://www.dupont.com/content/dam/electronics/amer/us/en/electronics/public/documents/en/EI-10206-Kapton-HN-Data-Sheet.pdf";
    addInsulationFilm(
        "MAT-DUPONT-NOMEX-410-50UM", "DuPont", "Nomex 410", "aramid insulation paper",
        nomex410Source, 50.0e-6, 18.0e6, "ASTM D149 AC rapid rise", "220 C (UL insulation recognition)",
        "MANUFACTURER_DATASHEET_THICKNESS_ROW",
        "0.05 mm grade. 18 kV/mm is a typical short-term AC rapid-rise value, not a continuous working stress. DuPont's dry-type continuous-stress recommendation is a separate system-level limit; verify exact construction, humidity and application.");
    addInsulationFilm(
        "MAT-DUPONT-NOMEX-410-130UM", "DuPont", "Nomex 410", "aramid insulation paper",
        nomex410Source, 130.0e-6, 28.0e6, "ASTM D149 AC rapid rise", "220 C (UL insulation recognition)",
        "MANUFACTURER_DATASHEET_THICKNESS_ROW",
        "0.13 mm grade. 28 kV/mm is a typical short-term AC rapid-rise value, not a continuous working stress. Verify paper direction, moisture conditioning, insulation system and production tolerance.");
    addInsulationFilm(
        "MAT-DUPONT-NOMEX-410-380UM", "DuPont", "Nomex 410", "aramid insulation paper",
        nomex410Source, 380.0e-6, 33.0e6, "ASTM D149 AC rapid rise", "220 C (UL insulation recognition)",
        "MANUFACTURER_DATASHEET_THICKNESS_ROW",
        "0.38 mm grade. 33 kV/mm is a typical short-term AC rapid-rise value, not a continuous working stress. Verify paper direction, moisture conditioning, insulation system and production tolerance.");
    addInsulationFilm(
        "MAT-DUPONT-KAPTON-HN-125UM", "DuPont", "Kapton HN", "polyimide film",
        kaptonHnSource, 12.5e-6, 315.0e6, "ASTM D149 at 60 Hz", "",
        "MANUFACTURER_DATASHEET_THICKNESS_ROW",
        "12.7 um nominal gauge (0.5 mil). 315 V/um is typical short-term dielectric strength at the stated test conditions; not a continuous design field or guaranteed minimum. Thermal service limit depends on the complete construction and application.");
    addInsulationFilm(
        "MAT-DUPONT-KAPTON-HN-50UM", "DuPont", "Kapton HN", "polyimide film",
        kaptonHnSource, 50.0e-6, 240.0e6, "ASTM D149 at 60 Hz", "",
        "MANUFACTURER_DATASHEET_THICKNESS_ROW",
        "50 um nominal gauge (2 mil). 240 V/um is typical short-term dielectric strength at the stated test conditions; not a continuous design field or guaranteed minimum. Thermal service limit depends on the complete construction and application.");

    const std::string alleimaIsomidSource =
        "https://www.alleima.com/en/technical-center/material-datasheets/wire-insulations-and-coatings/isomid/";
    const json isomid = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "ALLEIMA-ISOMID"}, {"manufacturer", "Alleima"},
            {"grade", "Isomid"}, {"family", "THEIC-modified polyesterimide wire enamel"},
            {"thermalClass_C", 200},
            {"standards", json::array({"NEMA MW 30", "NEMA MW 74"})},
            {"dielectricStrengthAt25C_V", {{"MW30_18AWG", 13000.0}, {"MW74_18AWG", 13000.0}}},
            {"cutThroughTemperature_C", 370.0},
            {"testWireGauge", "18 AWG"}
        }},
        {"sourceDocuments", json::array({alleimaIsomidSource})},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_DATASHEET"},
            {"dielectricStrengthStatus", "TEST_VALUE_FOR_SPECIFIED_18AWG_WIRE_CONSTRUCTION"},
            {"wireSizeStatus", "NOT_A_GENERAL_FILM_THICKNESS_VALUE"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Isomid is a thermal class 200 basecoat enamel. The 13 kV breakdown value applies to the specified 18 AWG wire construction and must not be converted to a generic dielectric field. Not directly solderable; enamel removal is required for connections. Topcoat combinations change friction and electrical properties."}
        }}
    };
    records.push_back(SeedRecord{
        "MAT-ALLEIMA-ISOMID", "SEED-MAT-ALLEIMA-ISOMID-R1", "insulation-material",
        alleimaIsomidSource, "Alleima Isomid thermal class 200 wire enamel", isomid
    });

    const std::string elantasPowerGenSource =
        "https://elantascomcdn.azureedge.net/fileadmin/elantas/press_news/Download-Center-ELANTAS-Europe/2025/PowerGenerationImpregnation_CastingPotting/Power_Generation_Impregnation_Casting_Potting.pdf";
    const json up142 = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "ELANTAS-ELAN-PROTECT-UP142-TX"},
            {"manufacturer", "ELANTAS"}, {"grade", "ELAN-protect UP142 TX"},
            {"family", "impregnating resin"}, {"chemistry", "unsaturated polyester resin"},
            {"thermalIndex_C_at_20000h", 200.0},
            {"viscosityAt23C_mPa_s", {{"minimum", 600.0}, {"maximum", 1400.0}}},
            {"process", "V(P)I"}
        }},
        {"sourceDocuments", json::array({elantasPowerGenSource})},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_POWER_GENERATION_PRODUCT_GUIDE"},
            {"thermalIndexStatus", "MANUFACTURER_TI_20000H"},
            {"viscosityStatus", "MANUFACTURER_RANGE_AT_23C"},
            {"dielectricPropertiesStatus", "NOT_IMPORTED"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Thermal index and viscosity are product-guide values, not a complete cured-resin property set. Obtain current TDS/SDS, cure schedule, compatibility and electrical test data for the selected production batch before design release."}
        }}
    };
    records.push_back(SeedRecord{
        "MAT-ELANTAS-UP142-TX", "SEED-MAT-ELANTAS-UP142-TX-R1", "insulation-material",
        elantasPowerGenSource, "ELANTAS ELAN-protect UP142 TX VPI impregnating resin", up142
    });

    const std::string elmoglasSource =
        "https://pt.elantas.com/fileadmin/elantas/companies/elantas_beck_india/products/4867_elantas_electrical_resins___varnishes_bro_sep_17.pdf";
    const json elmoglas = {
        {"schemaVersion", 1},
        {"material", {
            {"id", "ELANTAS-ELMOGLAS-H69A"},
            {"manufacturer", "ELANTAS"}, {"grade", "Elmoglas H 69 A"},
            {"family", "polyesterimide binder varnish"},
            {"thermalClass_C", 180},
            {"application", "binder varnish for polyester-film / glass / mica tapes and GFCC"},
            {"curingTemperature_C", {300.0, 400.0}},
            {"curingTime_minutes", 3.0}
        }},
        {"sourceDocuments", json::array({elmoglasSource})},
        {"dataQuality", {
            {"sourceStatus", "MANUFACTURER_PRODUCT_SELECTION_GUIDE"},
            {"thermalClassStatus", "MANUFACTURER_LISTED_CLASS_180"},
            {"cureStatus", "MANUFACTURER_LISTED_GUIDE_VALUE"},
            {"dielectricPropertiesStatus", "NOT_IMPORTED"},
            {"designReleaseStatus", "NOT_VERIFIED"},
            {"notes", "Binder varnish guide entry for composite insulation tapes. Cure line conditions must be confirmed against current product TDS and actual conductor thermal mass; no dielectric strength or continuous field is inferred."}
        }}
    };
    records.push_back(SeedRecord{
        "MAT-ELANTAS-ELMOGLAS-H69A", "SEED-MAT-ELANTAS-ELMOGLAS-H69A-R1", "insulation-material",
        elmoglasSource, "ELANTAS Elmoglas H 69 A polyesterimide binder varnish", elmoglas
    });


    // Powder, nanocrystalline and amorphous soft-magnetic materials.
    // Loss points are retained only with their published test conditions.
    const auto addMagneticMaterial = [&records](
        const std::string& id, const std::string& manufacturer,
        const std::string& grade, const std::string& family,
        const std::string& source, json properties,
        const std::string& sourceStatus, const std::string& notes) {
        properties["id"] = id;
        properties["manufacturer"] = manufacturer;
        properties["grade"] = grade;
        properties["family"] = family;
        const json payload = {
            {"schemaVersion", 1},
            {"material", std::move(properties)},
            {"sourceDocuments", json::array({source})},
            {"dataQuality", {
                {"sourceStatus", sourceStatus},
                {"bhCurveStatus", "NOT_IMPORTED"},
                {"coreLossStatus", "CONDITION_SPECIFIC_VALUES_ONLY"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", notes}
            }}
        };
        records.push_back(SeedRecord{
            id, "SEED-" + id + "-R1", "core-material",
            source, manufacturer + " " + grade + " soft magnetic material",
            payload
        });
    };

    const std::string micrometalsMix2Source =
        "https://datasheets.micrometals.com/D101-1002-DataSheet.pdf";
    addMagneticMaterial(
        "MAT-MICROMETALS-MIX-2", "Micrometals", "Mix-2",
        "distributed-gap iron powder",
        micrometalsMix2Source,
        json{
            {"initialPermeability_reference", 10.0},
            {"temperatureStability_ppm_per_K_typical", 95.0},
            {"coreLoss_W_per_m3", 18000.0},
            {"coreLossTest", {{"peakFluxDensity_T", 0.014}, {"frequency_Hz", 100000.0}}},
            {"coreLossTestFluxDensitySourceUnit", "140 gauss"}
        },
        "MANUFACTURER_PART_DATASHEET_D101-1002",
        "Mix-2 values are from Micrometals D101-1002 disc-core datasheet. The 18 mW/cm3 loss is specified at 140 gauss and 100 kHz only; it is not a general loss curve or a guaranteed value for every Mix-2 geometry. No saturation curve is inferred.");

    const std::string magneticsKoolMu14Source =
        "https://www.mag-inc.com/Media/Magnetics/File-Library/Product%20Literature/Powder%20Core%20Literature/Magnetics-14-Kool-Mu.pdf";
    addMagneticMaterial(
        "MAT-MAGNETICS-KOOL-MU-14U", "Magnetics", "Kool Mµ 14µ",
        "distributed-gap FeSiAl powder",
        magneticsKoolMu14Source,
        json{
            {"composition", {{"Fe_percent", 85.0}, {"Si_percent", 9.0}, {"Al_percent", 6.0}}},
            {"initialPermeability_nominal", 14.0},
            {"saturationFluxDensity_T", 1.05},
            {"curieTemperature_C", 500.0},
            {"operatingTemperature_C", {-55.0, 200.0}},
            {"coreLoss_W_per_m3", 600000.0},
            {"coreLossTest", {{"frequency_Hz", 50000.0}, {"peakFluxDensity_T", 0.1}}},
            {"dcBias_A_turns_per_cm", {{"permeabilityRetention_0p8", 175.0}, {"permeabilityRetention_0p7", 334.0}}}
        },
        "MANUFACTURER_14UM_PRODUCT_DATASHEET",
        "14µ grade-specific data. Core loss is typical at 50 kHz and 100 mT. DC-bias values retain the manufacturer's A·turns/cm unit. Do not transfer these figures to Kool Mµ MAX, Hf, Ultra or other permeability grades.");

    const std::string vitropermSource =
        "https://www.vacuumschmelze.com/products/soft-magnetic-materials-and-stamped-parts/nanocrystalline-material-vitroperm";
    const auto addVitroperm = [&](const std::string& grade, double saturationT,
                                  double permeabilityMin, double permeabilityMax,
                                  double coercivityMax, double magnetostrictionMin,
                                  double magnetostrictionMax, double thicknessUm,
                                  double crystallizationTemperatureC,
                                  double densityAmorphous, double densityNano,
                                  const json& lossData, const std::string& notes) {
        json properties = {
            {"materialTechnology", "nanocrystalline Fe-based ribbon"},
            {"saturationPolarization_T", saturationT},
            {"relativePermeability_range", {permeabilityMin, permeabilityMax}},
            {"coerciveField_max_A_per_m", coercivityMax},
            {"saturationMagnetostriction_ppm_range", {magnetostrictionMin, magnetostrictionMax}},
            {"ribbonThickness_um_nominal", thicknessUm},
            {"densityAmorphous_g_per_cm3", densityAmorphous},
            {"densityNanocrystalline_g_per_cm3", densityNano},
            {"electricalResistivity_ohm_m", 1.15e-6},
            {"curieTemperature_C", 600.0},
            {"crystallizationTemperature_C_typical", crystallizationTemperatureC},
            {"annealingRequired", true}
        };
        if (!lossData.is_null())
            properties["coreLossData"] = lossData;
        addMagneticMaterial(
            "MAT-VAC-VITROPERM-" + grade, "VACUUMSCHMELZE", "VITROPERM " + grade,
            "nanocrystalline soft magnetic ribbon", vitropermSource,
            std::move(properties), "MANUFACTURER_GRADE_DATA",
            notes + " Magnetic properties depend strongly on annealing field, stress and final core processing. The permeability and loss values apply only to the stated annealed condition; no generic B-H or loss curve is inferred.");
    };
    addVitroperm("220", 1.24, 1800.0, 2400.0, 5.0, 10.0, 11.0, 20.0, 460.0, 7.42, 7.62,
        nullptr, "Transverse-field annealed F-loop material; manufacturer reports low-permeability grade data.");
    addVitroperm("250", 1.24, 2800.0, 4000.0, 3.0, 8.0, 9.0, 20.0, 480.0, 7.35, 7.55,
        nullptr, "Transverse-field annealed F-loop material; manufacturer reports low-permeability grade data.");
    addVitroperm("270", 1.21, 4700.0, 5100.0, 3.0, 6.0, 7.0, 18.0, 480.0, 7.30, 7.50,
        nullptr, "Transverse-field annealed F-loop material; manufacturer reports low-permeability grade data.");
    addVitroperm("400", 1.30, 80000.0, 500000.0, 1.0, 3.0, 5.0, 18.0, 495.0, 7.20, 7.39,
        json{{"specificLoss_W_per_kg", 70.0},
             {"test", {{"frequency_Hz", 100000.0}, {"peakFluxDensity_T", 0.2},
                       {"annealingCondition", "R-annealed"}}},
             {"limitType", "maximum"}},
        "R-annealed material; maximum loss <=70 W/kg at 100 kHz and 0.2 T. Permeability values depend on measurement field and annealing.");
    addVitroperm("500-800", 1.24, 20000.0, 200000.0, 0.5, 0.0, 0.5, 18.0, 510.0, 7.17, 7.35,
        json{
            {"points", json::array({
                {{"specificLoss_W_per_kg", 80.0}, {"limitType", "maximum"},
                 {"frequency_Hz", 100000.0}, {"peakFluxDensity_T", 0.3},
                 {"annealingCondition", "F-annealed"}},
                {{"specificLoss_W_per_kg", 0.03}, {"limitType", "typical"},
                 {"frequency_Hz", 50.0}, {"peakFluxDensity_T", 1.0},
                 {"annealingCondition", "R-annealed"}}
            })}
        },
        "The 500/800 family values include distinct F- and R-annealed conditions; do not combine them into one loss curve or treat the 50 Hz point as an F-annealed value.");

    const std::string metglasSource =
        "https://metglas.com/wp-content/uploads/2021/06/Metglas-Power-Brochure-Updated.pdf";
    addMagneticMaterial(
        "MAT-METGLAS-2605SA1", "Metglas", "2605SA1",
        "amorphous Fe-based ribbon",
        metglasSource,
        json{
            {"saturationInduction_T", 1.56},
            {"electricalResistivity_ohm_m", 1.3e-6},
            {"saturationMagnetostriction_ppm", 27.0},
            {"curieTemperature_C", 395.0},
            {"density_g_per_cm3", 7.18},
            {"crystallizationTemperature_C", 510.0},
            {"tensileStrength_MPa", 2000.0},
            {"youngsModulus_GPa", 110.0},
            {"propertyValuesAreGuaranteed", false}
        },
        "MANUFACTURER_POWER_BROCHURE_TYPICAL_UNGUARANTEED",
        "Manufacturer explicitly marks these general properties as not guaranteed. Magnetic loss and B-H data are not imported; they depend on ribbon processing, core geometry and annealing.");
    addMagneticMaterial(
        "MAT-METGLAS-2605HB1M", "Metglas", "2605HB1M",
        "amorphous Fe-based ribbon",
        metglasSource,
        json{
            {"saturationInduction_T", 1.63},
            {"electricalResistivity_ohm_m", 1.2e-6},
            {"saturationMagnetostriction_ppm", 27.0},
            {"curieTemperature_C", 364.0},
            {"density_g_per_cm3", 7.33},
            {"crystallizationTemperature_C", 489.0},
            {"tensileStrength_MPa", 2100.0},
            {"youngsModulus_GPa", 120.0},
            {"propertyValuesAreGuaranteed", false}
        },
        "MANUFACTURER_POWER_BROCHURE_TYPICAL_UNGUARANTEED",
        "Manufacturer explicitly marks these general properties as not guaranteed. Magnetic loss and B-H data are not imported; they depend on ribbon processing, core geometry and annealing.");

    // Construction templates describe strand arrangements, not new bulk materials.
    // Electrical properties are deliberately inherited from the selected strand grade
    // and must account for transposition, insulation, proximity and skin effects.
    const std::vector<std::tuple<std::string, std::string, std::string, std::string>> conductorAssemblies{
        {"ASM-CU-LITZ-ROUND", "litz-wire", "bundle-of-insulated-round-strands",
            "https://www.elektrisola.com/Download/LitzWire_eng.pdf"},
        {"ASM-CU-PARALLEL-STRANDS", "parallel-strands", "parallel-round-or-rectangular-strands",
            "https://www.deangeliprodotti.com/wp-content/uploads/2025/07/DAP_PLDNCA_ENG_L.pdf"},
        {"ASM-CU-CTC", "continuously-transposed-conductor", "transposed-rectangular-strands",
            "https://www.deangeliprodotti.com/en/products/ctc-continuously-transposed-cable/"},
        {"ASM-CU-BRAID", "braided-flexible-conductor", "woven-round-strands",
            "https://www.deangeliprodotti.com/wp-content/uploads/2025/07/DAP_PLDNCA_ENG_L.pdf"}
    };
    for (const auto& [id, construction, geometry, assemblyReference] : conductorAssemblies) {
        const json payload = {
            {"schemaVersion", 1},
            {"component", {
                {"id", id}, {"conductorMaterial", "USER_SELECTED_COPPER_GRADE"},
                {"construction", construction}, {"strandGeometry", geometry},
                {"strandCount", "PARAMETRIC_USER_INPUT"},
                {"strandDiameter_m", "PARAMETRIC_USER_INPUT"},
                {"layLength_m", "PARAMETRIC_USER_INPUT"},
                {"individualStrandInsulation", "USER_SELECTED"},
                {"overallInsulation", "USER_SELECTED"},
                {"dimensionsAreManufacturerStockSizes", false}
            }},
            {"sourceDocuments", json::array({assemblyReference})},
            {"dataQuality", {
                {"sourceStatus", "GENERIC_ENGINEERING_CONSTRUCTION_TEMPLATE"},
                {"electricalModelStatus", "NOT_MODELED"},
                {"designReleaseStatus", "NOT_VERIFIED"},
                {"notes", "This is a construction template, not a manufacturer part or an electrical-property dataset. Require a supplier drawing and strand schedule. AC resistance, proximity loss, transposition effectiveness, packing factor, impregnation and insulation build must be modeled separately; do not substitute DC bulk-copper resistance for the assembled conductor."}
            }}
        };
        records.push_back(SeedRecord{
            id, "SEED-" + id + "-R1", "conductor-assembly",
            assemblyReference, "Parametric conductor construction template " + construction,
            payload
        });
    }

    return records;
}

} // namespace

Result<std::size_t> seedStarterMaterialCatalog(
    SqliteMaterialRevisionStore& store,
    const std::string& actor,
    const std::string& timestampUtc)
{
    if (actor.empty() || timestampUtc.empty())
        return Result<std::size_t>::failure(seedError("Seed actor and timestamp are required."));

    std::size_t inserted = 0;
    for (const auto& seed : starterRecords()) {
        const auto existing = store.latestRevision(seed.materialId);
        if (!existing)
            return Result<std::size_t>::failure(existing.error());
        if (existing.value())
            continue;

        const std::string serialized = seed.payload.dump();
        const auto canonical = canonicalizeJson(serialized);
        if (!canonical)
            return Result<std::size_t>::failure(canonical.error());
        const auto hash = sha256Hex(canonical.value());
        if (!hash)
            return Result<std::size_t>::failure(hash.error());

        MaterialRevision revision{
            seed.revisionId, seed.materialId, 1, std::nullopt, seed.recordType,
            canonical.value(), hash.value(), seed.sourceReference, seed.datasetRevision,
            actor, timestampUtc
        };
        const auto saved = store.appendRevision(revision);
        if (!saved)
            return Result<std::size_t>::failure(saved.error());
        ++inserted;
    }
    return Result<std::size_t>::success(inserted);
}

} // namespace p135::infrastructure::sqlite
