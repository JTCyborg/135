#include "p135/infrastructure/sqlite/starter_material_catalog.hpp"

#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
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

    return {
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
