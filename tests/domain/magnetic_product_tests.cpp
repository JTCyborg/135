#include "p135/domain/magnetic_product.hpp"
#include "p135/domain/material_catalog.hpp"

#include <cassert>

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
