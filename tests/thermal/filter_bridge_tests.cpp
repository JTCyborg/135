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

    return 0;
}
