#include "p135/domain/component_catalog.hpp"

#include <cassert>

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

    return 0;
}
