#include "p135/thermal/filter_bridge.hpp"

#include <cassert>
#include <cmath>

using namespace p135;
using namespace p135::thermal;

namespace {

domain::ResistorComponent makeResistor()
{
    domain::ResistorComponent resistor;
    resistor.id = "RES-DAMP-001";
    resistor.manufacturer = "Example manufacturer";
    resistor.series = "Power series";
    resistor.partNumber = "R100W";
    resistor.dataSource = domain::MaterialDataSource::ManufacturerDatasheet;
    resistor.sourceReference = "datasheet:example-resistor";
    resistor.datasetRevision = "rev-4";
    resistor.technology = domain::ResistorTechnology::Wirewound;
    resistor.resistance = units::Resistance{10.0};
    resistor.toleranceFraction = 0.05;
    resistor.ratedPower = units::Power{100.0};
    resistor.maximumWorkingVoltage = units::Voltage{1000.0};
    resistor.minimumTemperature = units::TemperatureKelvin{233.15};
    resistor.maximumTemperature = units::TemperatureKelvin{473.15};
    return resistor;
}

filter::HarmonicAnalysisResult makeHarmonicResult(double lossW)
{
    filter::HarmonicAnalysisResult result;
    result.branchStresses.push_back(filter::BranchHarmonicStress{
        5, 4.0, 100.0, 50.0, 40.0, lossW
    });
    return result;
}

} // namespace

int main()
{
    domain::ResistorCatalog catalog;
    const auto resistor = makeResistor();
    assert(catalog.add(resistor));

    const auto harmonic = makeHarmonicResult(16.0);
    DampingResistorThermalAssignment assignment;
    assignment.branchId = 5;
    assignment.resistorComponentId = resistor.id;
    assignment.thermalResistanceKPerW = 1.25;
    assignment.thermalResistanceSourceReference = "measured-assembly-Rtheta:rev-2";

    const auto result = analyzeDampingResistorThermals(
        harmonic, {assignment}, catalog, units::TemperatureKelvin{298.15}, "study-2026-11-19-r1");
    assert(result);
    const auto& node = result.value().nodes.front();
    assert(node.status == AssessmentStatus::Pass);
    assert(node.estimatedTemperature);
    assert(std::abs(node.estimatedTemperature->si() - 318.15) < 1.0e-9);
    assert(node.loss);
    assert(std::abs(node.loss->si() - 16.0) < 1.0e-9);

    assignment.thermalResistanceKPerW.reset();
    const auto incomplete = analyzeDampingResistorThermals(
        harmonic, {assignment}, catalog, units::TemperatureKelvin{298.15}, "study-2026-11-19-r1");
    assert(incomplete);
    assert(incomplete.value().nodes.front().status == AssessmentStatus::NotVerified);
    assert(!incomplete.value().nodes.front().estimatedTemperature);

    assignment.thermalResistanceKPerW = 20.0;
    const auto overheated = analyzeDampingResistorThermals(
        makeHarmonicResult(10.0), {assignment}, catalog,
        units::TemperatureKelvin{298.15}, "study-2026-11-19-r2");
    assert(overheated);
    assert(overheated.value().nodes.front().status == AssessmentStatus::Fail);

    const auto missingRevision = analyzeDampingResistorThermals(
        harmonic, {assignment}, catalog, units::TemperatureKelvin{298.15}, "");
    assert(!missingRevision);

    assignment.branchId = 99;
    const auto missingBranch = analyzeDampingResistorThermals(
        harmonic, {assignment}, catalog, units::TemperatureKelvin{298.15}, "study-r3");
    assert(!missingBranch);

    assignment.branchId = 5;
    assignment.resistorComponentId = "UNKNOWN";
    const auto missingComponent = analyzeDampingResistorThermals(
        harmonic, {assignment}, catalog, units::TemperatureKelvin{298.15}, "study-r3");
    assert(!missingComponent);


    {
        domain::CapacitorCatalog capacitorCatalog;
        domain::CapacitorComponent capacitor;
        capacitor.id = "CAP-FILTER-001";
        capacitor.manufacturer = "Example capacitor maker";
        capacitor.series = "Film series";
        capacitor.partNumber = "MKP-100UF";
        capacitor.dataSource = domain::MaterialDataSource::ManufacturerDatasheet;
        capacitor.sourceReference = "datasheet:example-film-cap";
        capacitor.datasetRevision = "cap-rev-7";
        capacitor.dielectric = domain::CapacitorDielectric::PolypropyleneFilm;
        capacitor.technology = domain::CapacitorTechnology::MetallizedFilm;
        capacitor.capacitance = units::Capacitance{100.0e-6};
        capacitor.toleranceFraction = 0.05;
        capacitor.ratedVoltage = units::Voltage{1000.0};
        capacitor.voltageRatingIsAC = true;
        capacitor.esrFrequencyCurve = {
            {units::Frequency{50.0}, units::Resistance{0.1}},
            {units::Frequency{250.0}, units::Resistance{0.2}},
            {units::Frequency{350.0}, units::Resistance{0.25}}
        };
        capacitor.rippleCurrent = units::Current{4.0};
        capacitor.minimumTemperature = units::TemperatureKelvin{233.15};
        capacitor.maximumTemperature = units::TemperatureKelvin{358.15};
        assert(capacitorCatalog.add(capacitor));

        filter::HarmonicAnalysisResult harmonicWithCapCurrents;
        harmonicWithCapCurrents.branchStresses.push_back(
            filter::BranchHarmonicStress{7, std::sqrt(13.0), 100.0, 50.0, 40.0, 0.0});
        harmonicWithCapCurrents.branchPoints = {
            {7, 1, units::Frequency{50.0}, 2.0},
            {7, 5, units::Frequency{250.0}, 3.0}
        };

        FilterCapacitorThermalAssignment capAssignment;
        capAssignment.branchId = 7;
        capAssignment.capacitorComponentId = capacitor.id;
        capAssignment.thermalResistanceKPerW = 5.0;
        capAssignment.thermalResistanceSourceReference = "assembly-test-Rtheta:rev-1";

        const auto capThermal = analyzeFilterCapacitorThermals(
            harmonicWithCapCurrents, {capAssignment}, capacitorCatalog,
            units::TemperatureKelvin{298.15}, "harmonics-cap-test-r1");
        assert(capThermal);
        const auto& cap = capThermal.value().capacitors.front();
        assert(cap.rippleCurrentStatus == AssessmentStatus::Pass);
        assert(std::abs(cap.rippleCurrentRmsA - std::sqrt(13.0)) < 1.0e-9);
        assert(cap.esrLoss);
        assert(std::abs(cap.esrLoss->si() - 2.2) < 1.0e-9);
        assert(cap.thermal.status == AssessmentStatus::Pass);
        assert(cap.thermal.estimatedTemperature);
        assert(std::abs(cap.thermal.estimatedTemperature->si() - 309.15) < 1.0e-9);

        auto lowRippleCap = capacitor;
        lowRippleCap.id = "CAP-LOW-RIPPLE";
        lowRippleCap.rippleCurrent = units::Current{3.5};
        domain::CapacitorCatalog lowRippleCatalog;
        assert(lowRippleCatalog.add(lowRippleCap));
        capAssignment.capacitorComponentId = lowRippleCap.id;
        const auto rippleFail = analyzeFilterCapacitorThermals(
            harmonicWithCapCurrents, {capAssignment}, lowRippleCatalog,
            units::TemperatureKelvin{298.15}, "harmonics-cap-test-r2");
        assert(rippleFail);
        assert(rippleFail.value().capacitors.front().rippleCurrentStatus == AssessmentStatus::Fail);

        auto unratedCap = capacitor;
        unratedCap.id = "CAP-NO-RIPPLE-RATING";
        unratedCap.rippleCurrent.reset();
        domain::CapacitorCatalog unratedCatalog;
        assert(unratedCatalog.add(unratedCap));
        capAssignment.capacitorComponentId = unratedCap.id;
        const auto missingRippleRating = analyzeFilterCapacitorThermals(
            harmonicWithCapCurrents, {capAssignment}, unratedCatalog,
            units::TemperatureKelvin{298.15}, "harmonics-cap-test-r2b");
        assert(missingRippleRating);
        assert(missingRippleRating.value().capacitors.front().rippleCurrentStatus ==
               AssessmentStatus::NotVerified);

        capAssignment.capacitorComponentId = capacitor.id;
        capAssignment.thermalResistanceKPerW.reset();
        const auto missingRtheta = analyzeFilterCapacitorThermals(
            harmonicWithCapCurrents, {capAssignment}, capacitorCatalog,
            units::TemperatureKelvin{298.15}, "harmonics-cap-test-r3");
        assert(missingRtheta);
        assert(missingRtheta.value().capacitors.front().thermal.status == AssessmentStatus::NotVerified);

        auto insufficientCurve = capacitor;
        insufficientCurve.id = "CAP-ESR-GAP";
        insufficientCurve.esrFrequencyCurve.resize(1);
        domain::CapacitorCatalog insufficientCurveCatalog;
        assert(insufficientCurveCatalog.add(insufficientCurve));
        capAssignment.thermalResistanceKPerW = 5.0;
        capAssignment.capacitorComponentId = insufficientCurve.id;
        const auto noExtrapolation = analyzeFilterCapacitorThermals(
            harmonicWithCapCurrents, {capAssignment}, insufficientCurveCatalog,
            units::TemperatureKelvin{298.15}, "harmonics-cap-test-r4");
        assert(noExtrapolation);
        assert(!noExtrapolation.value().capacitors.front().esrLoss);
        assert(noExtrapolation.value().capacitors.front().thermal.status == AssessmentStatus::NotVerified);

        auto unconfirmedConstant = capacitor;
        unconfirmedConstant.id = "CAP-CONSTANT-UNCONFIRMED";
        unconfirmedConstant.esrFrequencyCurve.clear();
        unconfirmedConstant.esr = units::Resistance{0.1};
        unconfirmedConstant.esrConfirmedFrequencyIndependent = false;
        domain::CapacitorCatalog unconfirmedCatalog;
        assert(unconfirmedCatalog.add(unconfirmedConstant));
        capAssignment.capacitorComponentId = unconfirmedConstant.id;
        const auto constantNotTrusted = analyzeFilterCapacitorThermals(
            harmonicWithCapCurrents, {capAssignment}, unconfirmedCatalog,
            units::TemperatureKelvin{298.15}, "harmonics-cap-test-r5");
        assert(constantNotTrusted);
        assert(!constantNotTrusted.value().capacitors.front().esrLoss);
        assert(constantNotTrusted.value().capacitors.front().thermal.status == AssessmentStatus::NotVerified);
    }

    return 0;
}
