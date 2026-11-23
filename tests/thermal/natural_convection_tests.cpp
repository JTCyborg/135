#include "p135/thermal/natural_convection.hpp"

#include <cassert>
#include <cmath>

using namespace p135;
using namespace p135::thermal;


void testHorizontalPlateCorrelations()
{
    HorizontalPlateNaturalConvectionRequest request;
    request.exposedArea = units::Area{0.25};
    request.exposedPerimeter = units::Length{2.0};
    request.ambientTemperature = units::TemperatureKelvin{313.15};
    request.surfaceTemperature = units::TemperatureKelvin{353.15};
    request.fluid = NaturalConvectionFluidProperties{
        0.0275, 1.7e-5, 2.4e-5, 0.71, "TEST-AIR-PROPERTIES"
    };
    request.geometrySourceReference = "TEST-PLATE-GEOMETRY";
    request.unobstructedQuiescentAmbient = true;

    const auto upward = analyzeHorizontalPlateNaturalConvection(request);
    assert(upward);
    assert(upward.value().rayleighNumber >= 1.0e4);
    assert(upward.value().rayleighNumber <= 1.0e11);
    assert(upward.value().nusseltNumber > 0.0);
    assert(upward.value().thermalResistanceKPerW > 0.0);
    assert(upward.value().status == "PRELIMINARY");

    request.orientation = HorizontalPlateOrientation::HeatedFaceDownward;
    const auto downward = analyzeHorizontalPlateNaturalConvection(request);
    assert(downward);
    assert(downward.value().rayleighNumber >= 1.0e5);
    assert(downward.value().rayleighNumber <= 1.0e10);

    request.unobstructedQuiescentAmbient = false;
    const auto obstructed = analyzeHorizontalPlateNaturalConvection(request);
    assert(!obstructed);
}
 
int main()
{
    testHorizontalPlateCorrelations();
    VerticalPlateNaturalConvectionRequest request;
    request.exposedArea = units::Area{0.12};
    request.verticalCharacteristicLength = units::Length{0.30};
    request.ambientTemperature = units::TemperatureKelvin{313.15};
    request.surfaceTemperature = units::TemperatureKelvin{353.15};
    request.fluid.thermalConductivityWPerMK = 0.0275;
    request.fluid.kinematicViscosityM2PerS = 1.7e-5;
    request.fluid.thermalDiffusivityM2PerS = 2.4e-5;
    request.fluid.prandtlNumber = 0.71;
    request.fluid.sourceReference = "test-fixture:air-properties";
    request.geometrySourceReference = "test-fixture:vertical-plate";
    request.unobstructedQuiescentAmbient = true;

    {
        const auto result = analyzeVerticalPlateNaturalConvection(request);
        assert(result);
        assert(result.value().status == "PRELIMINARY");
        assert(result.value().rayleighNumber > 0.0);
        assert(result.value().nusseltNumber > 0.0);
        assert(result.value().heatTransferCoefficientWPerM2K > 0.0);
        assert(result.value().thermalResistanceKPerW > 0.0);
        assert(result.value().correlation == "Churchill-Chu vertical isothermal plate");
    }
    {
        auto invalid = request;
        invalid.unobstructedQuiescentAmbient = false;
        assert(!analyzeVerticalPlateNaturalConvection(invalid));
    }
    {
        auto invalid = request;
        invalid.fluid.sourceReference.clear();
        assert(!analyzeVerticalPlateNaturalConvection(invalid));
    }
    {
        auto invalid = request;
        invalid.surfaceTemperature = invalid.ambientTemperature;
        assert(!analyzeVerticalPlateNaturalConvection(invalid));
    }
    {
        auto invalid = request;
        invalid.verticalCharacteristicLength = units::Length{1.0e-9};
        assert(!analyzeVerticalPlateNaturalConvection(invalid));
    }
    {
        auto invalid = request;
        invalid.fluid.prandtlNumber = 1.2;
        assert(!analyzeVerticalPlateNaturalConvection(invalid));
    }
    return 0;
}
