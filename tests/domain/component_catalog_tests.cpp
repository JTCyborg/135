#include "p135/domain/component_catalog.hpp"

#include <cassert>
#include <limits>

using namespace p135;
using namespace p135::domain;

int main()
{
    CapacitorCatalog catalog;
    CapacitorComponent capacitor;
    capacitor.id = "CAP-TEST-001";
    capacitor.manufacturer = "Test manufacturer";
    capacitor.series = "Test series";
    capacitor.partNumber = "TEST-10UF-1KV";
    capacitor.dataSource = MaterialDataSource::ManufacturerDatasheet;
    capacitor.sourceReference = "synthetic unit-test fixture";
    capacitor.datasetRevision = "fixture-1";
    capacitor.dielectric = CapacitorDielectric::PolypropyleneFilm;
    capacitor.technology = CapacitorTechnology::MetallizedFilm;
    capacitor.capacitance = units::Capacitance{10.0e-6};
    capacitor.toleranceFraction = 0.05;
    capacitor.ratedVoltage = units::Voltage{1000.0};
    capacitor.voltageRatingIsAC = true;
    capacitor.esr = units::Resistance{0.015};
    capacitor.esrFrequencyCurve = {
        {units::Frequency{50.0}, units::Resistance{0.1}},
        {units::Frequency{500.0}, units::Resistance{0.01}}
    };
    capacitor.esl = units::Inductance{35.0e-9};
    capacitor.dissipationFactor = 0.001;
    capacitor.rippleCurrent = units::Current{12.0};
    capacitor.minimumTemperature = units::TemperatureKelvin{233.15};
    capacitor.maximumTemperature = units::TemperatureKelvin{358.15};
    capacitor.ratedLife = units::Time{100000.0 * 3600.0};

    assert(validate(capacitor));
    assert(catalog.add(capacitor));
    assert(catalog.size() == 1);
    assert(catalog.find("CAP-TEST-001") != nullptr);
    assert(!catalog.add(capacitor));

    const auto interpolatedEsr = interpolateCapacitorEsr(
        capacitor, units::Frequency{std::sqrt(50.0 * 500.0)});
    assert(interpolatedEsr);
    assert(std::abs(interpolatedEsr.value().si() - std::sqrt(0.1 * 0.01)) < 1.0e-12);
    assert(!interpolateCapacitorEsr(capacitor, units::Frequency{10.0}));

    auto invalidEsrCurve = capacitor;
    invalidEsrCurve.id = "CAP-BAD-ESR-CURVE";
    invalidEsrCurve.esrFrequencyCurve[1].frequency = units::Frequency{25.0};
    assert(!validate(invalidEsrCurve));

    auto unconfirmedConstantEsr = capacitor;
    unconfirmedConstantEsr.esrFrequencyCurve.clear();
    unconfirmedConstantEsr.esrConfirmedFrequencyIndependent = false;
    assert(!interpolateCapacitorEsr(unconfirmedConstantEsr, units::Frequency{50.0}));
    unconfirmedConstantEsr.esrConfirmedFrequencyIndependent = true;
    assert(interpolateCapacitorEsr(unconfirmedConstantEsr, units::Frequency{50.0}));

    auto invalidCapacitance = capacitor;
    invalidCapacitance.id = "CAP-BAD-C";
    invalidCapacitance.capacitance = units::Capacitance{-1.0};
    assert(!validate(invalidCapacitance));

    auto invalidTolerance = capacitor;
    invalidTolerance.id = "CAP-BAD-TOL";
    invalidTolerance.toleranceFraction = 1.0;
    assert(!validate(invalidTolerance));

    auto invalidVoltage = capacitor;
    invalidVoltage.id = "CAP-BAD-V";
    invalidVoltage.ratedVoltage = units::Voltage{};
    assert(!validate(invalidVoltage));

    auto invalidEsr = capacitor;
    invalidEsr.id = "CAP-BAD-ESR";
    invalidEsr.esr = units::Resistance{-0.1};
    assert(!validate(invalidEsr));

    auto missingProvenance = capacitor;
    missingProvenance.id = "CAP-NO-SOURCE";
    missingProvenance.sourceReference.clear();
    assert(!validate(missingProvenance));

    auto missingOptionalData = capacitor;
    missingOptionalData.id = "CAP-OPTIONAL-MISSING";
    missingOptionalData.esr.reset();
    missingOptionalData.esl.reset();
    missingOptionalData.dissipationFactor.reset();
    missingOptionalData.rippleCurrent.reset();
    assert(validate(missingOptionalData)); // structural validity; thermal/loss use remains NOT_VERIFIED

    ResistorCatalog resistorCatalog;
    ResistorComponent resistor;
    resistor.id = "RES-TEST-001";
    resistor.manufacturer = "Test manufacturer";
    resistor.series = "Pulse series";
    resistor.partNumber = "TEST-10R-100W";
    resistor.dataSource = MaterialDataSource::ManufacturerDatasheet;
    resistor.sourceReference = "synthetic unit-test fixture";
    resistor.datasetRevision = "fixture-1";
    resistor.technology = ResistorTechnology::Wirewound;
    resistor.resistance = units::Resistance{10.0};
    resistor.toleranceFraction = 0.01;
    resistor.ratedPower = units::Power{100.0};
    resistor.maximumWorkingVoltage = units::Voltage{1000.0};
    resistor.temperatureCoefficient = units::TemperatureCoefficient{100.0e-6};
    resistor.parasiticInductance = units::Inductance{2.0e-6};
    resistor.parasiticCapacitance = units::Capacitance{5.0e-12};
    resistor.singlePulseEnergyJoules = 25.0;
    resistor.minimumTemperature = units::TemperatureKelvin{233.15};
    resistor.maximumTemperature = units::TemperatureKelvin{473.15};

    assert(validate(resistor));
    assert(resistorCatalog.add(resistor));
    assert(resistorCatalog.size() == 1);
    assert(resistorCatalog.find("RES-TEST-001") != nullptr);
    assert(!resistorCatalog.add(resistor));

    auto invalidResistance = resistor;
    invalidResistance.id = "RES-BAD-R";
    invalidResistance.resistance = units::Resistance{-1.0};
    assert(!validate(invalidResistance));

    auto invalidPower = resistor;
    invalidPower.id = "RES-BAD-P";
    invalidPower.ratedPower = units::Power{};
    assert(!validate(invalidPower));

    auto invalidTempco = resistor;
    invalidTempco.id = "RES-BAD-TCR";
    invalidTempco.temperatureCoefficient =
        units::TemperatureCoefficient{std::numeric_limits<double>::quiet_NaN()};
    assert(!validate(invalidTempco));

    auto optionalPulseUnknown = resistor;
    optionalPulseUnknown.id = "RES-NO-PULSE-DATA";
    optionalPulseUnknown.singlePulseEnergyJoules.reset();
    optionalPulseUnknown.parasiticInductance.reset();
    optionalPulseUnknown.parasiticCapacitance.reset();
    assert(validate(optionalPulseUnknown));

    return 0;
}
