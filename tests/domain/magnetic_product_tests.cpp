#include "p135/domain/magnetic_product.hpp"
#include "p135/domain/magnetic_circuit.hpp"
#include "p135/domain/material_catalog.hpp"

#include <cassert>
#include <cmath>

using namespace p135;
using namespace p135::domain;

namespace {
MagneticProduct validProduct()
{
    MagneticProduct product;
    product.name = "Test inductor";
    product.core.effectiveArea = units::Area{4.0e-4};
    product.core.windowArea = units::Area{8.0e-4};
    product.core.meanMagneticPath = units::Length{0.25};
    product.core.airGap = units::Length{1.0e-3};
    product.core.stackingFactor = 0.95;
    product.coreMaterial.id = "MAT-TEST";
    product.coreMaterial.grade = "Test grade";
    product.coreMaterial.manufacturer = "Test manufacturer";
    product.coreMaterial.sourceReference = "test fixture";
    product.coreMaterial.datasetRevision = "1";
    product.coreMaterial.density = units::Density{7650.0};
    product.coreMaterial.bhCurve = {
        {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
        {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.8}},
        {units::MagneticFieldStrength{500.0}, units::FluxDensity{1.2}}
    };
    Winding winding;
    winding.name = "Main";
    winding.turns = 100;
    winding.turnsPerLayer = 25;
    winding.layers = 4;
    winding.conductor.id = "CU-1";
    winding.conductor.bareCrossSection = units::Area{2.5e-6};
    winding.bobbin.material = "PBT";
    winding.bobbin.innerWidth = units::Length{0.04};
    winding.bobbin.innerHeight = units::Length{0.03};
    winding.bobbin.wallThickness = units::Length{0.001};
    product.windings.push_back(winding);
    return product;
}
}

int main()
{
    assert(validate(validProduct()));

    {
        MagneticCircuitRequest request;
        request.core.effectiveArea = units::Area{4.0e-4};
        request.core.meanMagneticPath = units::Length{0.25};
        request.core.airGap = units::Length{1.0e-3};
        request.turns = 100;
        request.current = units::Current{1.0};
        request.material = validProduct().coreMaterial;
        request.material.density = units::Density{}; // Density is not required by the B-H magnetic-circuit equations.
        request.material.bhCurve = {
            {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
            {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.5}},
            {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.0}}
        };
        const auto zeroCurrent = analyzeMagneticCircuit(request);
        assert(zeroCurrent);
        assert(std::abs(zeroCurrent.value().fluxDensity.si()) < 1.0e-12);
        assert(zeroCurrent.value().kneeCurrent.has_value());

        auto reverseCurrent = request;
        reverseCurrent.current = units::Current{-1.0};
        const auto reverseResult = analyzeMagneticCircuit(reverseCurrent);
        assert(reverseResult);

        const auto operating = analyzeMagneticCircuit(request);
        assert(operating);
        assert(operating.value().withinBhCurveRange);
        assert(std::abs(reverseResult.value().fluxDensity.si() -
                        operating.value().fluxDensity.si()) < 1.0e-12);
        assert(operating.value().fluxDensity.si() > 0.0);
        assert(operating.value().flux.si() > 0.0);
        assert(operating.value().secantInductance.si() > 0.0);
        assert(operating.value().differentialInductance.si() > 0.0);
        assert(operating.value().kneeFluxDensity.has_value());
        assert(operating.value().kneeCurrent.has_value());
        assert(operating.value().kneeUtilization.has_value());
        assert(std::abs(operating.value().kneeFluxDensity->si() - 0.5) < 1.0e-12);
        assert(operating.value().kneeCurrent->si() > 4.0);

        auto atKnee = request;
        atKnee.current = *operating.value().kneeCurrent;
        const auto kneeResult = analyzeMagneticCircuit(atKnee);
        assert(kneeResult);
        assert(std::abs(kneeResult.value().fluxDensity.si() - 0.5) < 1.0e-9);

        auto beyondCurve = request;
        beyondCurve.current = units::Current{11.0};
        const auto outOfDomain = analyzeMagneticCircuit(beyondCurve);
        assert(!outOfDomain);
        assert(outOfDomain.error().code == ErrorCode::ModelOutOfDomain);

        auto invalidRatio = request;
        invalidRatio.kneeSlopeRatio = 1.0;
        assert(!analyzeMagneticCircuit(invalidRatio));

        auto invalidTurns = request;
        invalidTurns.turns = 0;
        assert(!analyzeMagneticCircuit(invalidTurns));

        auto missingOrigin = request;
        missingOrigin.material.bhCurve.front().fluxDensity = units::FluxDensity{0.01};
        const auto missingOriginResult = analyzeMagneticCircuit(missingOrigin);
        assert(!missingOriginResult);
        assert(missingOriginResult.error().code == ErrorCode::MissingMaterialProperty);
    }


    CoreMaterialCatalog catalog;
    auto material = validProduct().coreMaterial;
    material.lossDataReference = "measured loss fixture";
    material.lossData = {{units::Frequency{50.0}, units::FluxDensity{1.0}, units::TemperatureKelvin{293.15}, units::CoreLossDensity{1.2}}};
    assert(catalog.add(material));
    assert(catalog.size() == 1);
    assert(catalog.find("MAT-TEST") != nullptr);
    assert(!catalog.add(material));
    auto invalidLoss = material;
    invalidLoss.id = "MAT-BAD";
    invalidLoss.lossData.front().specificLoss = units::CoreLossDensity{-1.0};
    assert(!catalog.add(invalidLoss));
    auto missingProvenance = material;
    missingProvenance.id = "MAT-NOPROV";
    missingProvenance.sourceReference.clear();
    assert(!validate(missingProvenance));

    ConductorMaterialCatalog conductorCatalog;
    ConductorMaterial copper;
    copper.id = "COND-CU-TEST";
    copper.grade = "Test copper";
    copper.sourceReference = "test fixture";
    copper.datasetRevision = "1";
    copper.resistivityAt20C = units::Resistivity{1.72e-8};
    copper.temperatureCoefficient = units::TemperatureCoefficient{0.0039};
    copper.density = units::Density{8960.0};
    assert(conductorCatalog.add(copper));
    auto negativeTemperatureCoefficient = copper;
    negativeTemperatureCoefficient.id = "COND-NTC-TEST";
    negativeTemperatureCoefficient.temperatureCoefficient = units::TemperatureCoefficient{-0.002};
    assert(validate(negativeTemperatureCoefficient));
    assert(conductorCatalog.size() == 1);
    assert(conductorCatalog.find("COND-CU-TEST") != nullptr);
    assert(!conductorCatalog.add(copper));
    auto invalidConductor = copper;
    invalidConductor.id = "COND-BAD";
    invalidConductor.resistivityAt20C = units::Resistivity{-1.0};
    assert(!validate(invalidConductor));

    InsulationMaterialCatalog insulationCatalog;
    InsulationMaterial insulation;
    insulation.id = "INS-TEST";
    insulation.grade = "Test film";
    insulation.sourceReference = "test fixture";
    insulation.datasetRevision = "1";
    insulation.thermalClass = InsulationThermalClass::F;
    insulation.maximumTemperature = units::TemperatureKelvin{428.15};
    insulation.dielectricStrength = units::ElectricFieldStrength{1.0e7};
    insulation.relativePermittivity = 3.2;
    assert(insulationCatalog.add(insulation));
    auto unspecifiedOptionalProperties = insulation;
    unspecifiedOptionalProperties.id = "INS-OPTIONAL-ABSENT";
    unspecifiedOptionalProperties.relativePermittivity.reset();
    unspecifiedOptionalProperties.density.reset();
    assert(validate(unspecifiedOptionalProperties));
    auto invalidPermittivity = insulation;
    invalidPermittivity.id = "INS-BAD-PERMITTIVITY";
    invalidPermittivity.relativePermittivity = 0.0;
    assert(!validate(invalidPermittivity));
    assert(insulationCatalog.size() == 1);
    assert(insulationCatalog.find("INS-TEST") != nullptr);
    assert(!insulationCatalog.add(insulation));
    auto missingInsulationData = insulation;
    missingInsulationData.id = "INS-MISSING";
    missingInsulationData.dielectricStrength = units::ElectricFieldStrength{};
    assert(!validate(missingInsulationData));

    auto missingBh = validProduct();
    missingBh.coreMaterial.bhCurve.clear();
    const auto missingBhResult = validate(missingBh);
    assert(!missingBhResult);
    assert(missingBhResult.error().code == ErrorCode::MissingMaterialProperty);

    auto insufficientWindow = validProduct();
    insufficientWindow.windings.front().turns = 101;
    insufficientWindow.windings.front().turnsPerLayer = 25;
    insufficientWindow.windings.front().layers = 4;
    assert(!validate(insufficientWindow));

    auto invalidStack = validProduct();
    invalidStack.core.stackingFactor = 1.1;
    assert(!validate(invalidStack));

    auto missingBobbin = validProduct();
    missingBobbin.windings.front().bobbin.material.clear();
    assert(!validate(missingBobbin));

    return 0;
}
