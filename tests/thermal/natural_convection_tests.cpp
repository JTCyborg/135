#include "p135/thermal/natural_convection.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <string>

using namespace p135;
using namespace p135::thermal;


void testVerticalPlateParameterMatrix()
{
    struct Scenario {
        double areaM2;
        double heightM;
        double ambientK;
        double surfaceK;
        double conductivity;
        double viscosity;
        double prandtl;
    };
    // Deliberately vary geometry, temperature lift and air-property inputs
    // between cases; alpha is derived from nu/Pr to keep the fluid tuple
    // internally consistent.
    constexpr std::array<Scenario, 4> scenarios{{
        {0.045, 0.12, 293.15, 313.15, 0.0242, 1.42e-5, 0.704},
        {0.120, 0.30, 303.15, 353.15, 0.0275, 1.70e-5, 0.710},
        {0.280, 0.48, 313.15, 373.15, 0.0291, 2.02e-5, 0.716},
        {0.650, 0.65, 323.15, 408.15, 0.0310, 2.48e-5, 0.722}
    }};

    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        const auto& scenario = scenarios[index];
        VerticalPlateNaturalConvectionRequest request;
        request.exposedArea = units::Area{scenario.areaM2};
        request.verticalCharacteristicLength = units::Length{scenario.heightM};
        request.ambientTemperature = units::TemperatureKelvin{scenario.ambientK};
        request.surfaceTemperature = units::TemperatureKelvin{scenario.surfaceK};
        request.fluid = NaturalConvectionFluidProperties{
            scenario.conductivity, scenario.viscosity,
            scenario.viscosity / scenario.prandtl, scenario.prandtl,
            "test-matrix:air-" + std::to_string(index)
        };
        request.geometrySourceReference = "test-matrix:plate-" + std::to_string(index);
        request.unobstructedQuiescentAmbient = true;

        const auto actual = analyzeVerticalPlateNaturalConvection(request);
        assert(actual);

        const double film = 0.5 * (scenario.ambientK + scenario.surfaceK);
        const double deltaT = scenario.surfaceK - scenario.ambientK;
        const double rayleigh = 9.80665 * (1.0 / film) * deltaT *
            std::pow(scenario.heightM, 3.0) /
            (scenario.viscosity * (scenario.viscosity / scenario.prandtl));
        const double denominator = std::pow(
            1.0 + std::pow(0.492 / scenario.prandtl, 9.0 / 16.0), 8.0 / 27.0);
        const double nusselt = std::pow(
            0.825 + 0.387 * std::pow(rayleigh, 1.0 / 6.0) / denominator, 2.0);
        const double coefficient = nusselt * scenario.conductivity / scenario.heightM;
        const double resistance = 1.0 / (coefficient * scenario.areaM2);

        const auto close = [](double lhs, double rhs) {
            return std::abs(lhs - rhs) <= 1.0e-10 * std::max(1.0, std::abs(rhs));
        };
        assert(close(actual.value().filmTemperatureK, film));
        assert(close(actual.value().rayleighNumber, rayleigh));
        assert(close(actual.value().nusseltNumber, nusselt));
        assert(close(actual.value().heatTransferCoefficientWPerM2K, coefficient));
        assert(close(actual.value().thermalResistanceKPerW, resistance));
        assert(actual.value().geometrySourceReference == request.geometrySourceReference);
        assert(actual.value().fluidPropertiesSourceReference == request.fluid.sourceReference);
        assert(actual.value().status == "PRELIMINARY");
    }
}

void testHorizontalPlateParameterMatrix()
{
    struct Scenario {
        double areaM2;
        double perimeterM;
        double ambientK;
        double surfaceK;
        double conductivity;
        double viscosity;
        double prandtl;
    };
    // Cover both McAdams upward-facing branches around Ra=1e7 and the
    // downward-facing range with independently varied plate/fluid inputs.
    constexpr std::array<Scenario, 4> scenarios{{
        {0.12, 1.40, 293.15, 313.15, 0.0242, 1.42e-5, 0.704},
        {0.25, 2.00, 303.15, 353.15, 0.0275, 1.70e-5, 0.710},
        {0.45, 2.80, 313.15, 373.15, 0.0291, 2.02e-5, 0.716},
        {0.65, 3.40, 323.15, 408.15, 0.0310, 2.48e-5, 0.722}
    }};

    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        const auto& scenario = scenarios[index];
        HorizontalPlateNaturalConvectionRequest request;
        request.exposedArea = units::Area{scenario.areaM2};
        request.exposedPerimeter = units::Length{scenario.perimeterM};
        request.ambientTemperature = units::TemperatureKelvin{scenario.ambientK};
        request.surfaceTemperature = units::TemperatureKelvin{scenario.surfaceK};
        request.fluid = NaturalConvectionFluidProperties{
            scenario.conductivity, scenario.viscosity,
            scenario.viscosity / scenario.prandtl, scenario.prandtl,
            "horizontal-matrix:air-" + std::to_string(index)
        };
        request.geometrySourceReference = "horizontal-matrix:plate-" + std::to_string(index);
        request.unobstructedQuiescentAmbient = true;

        const double characteristicLength = scenario.areaM2 / scenario.perimeterM;
        const double film = 0.5 * (scenario.ambientK + scenario.surfaceK);
        const double rayleigh = 9.80665 / film * (scenario.surfaceK - scenario.ambientK) *
            std::pow(characteristicLength, 3.0) /
            (scenario.viscosity * (scenario.viscosity / scenario.prandtl));
        assert(rayleigh >= 1.0e4 && rayleigh <= 1.0e11);

        for (const auto orientation : {HorizontalPlateOrientation::HeatedFaceUpward,
                                       HorizontalPlateOrientation::HeatedFaceDownward}) {
            request.orientation = orientation;
            const auto actual = analyzeHorizontalPlateNaturalConvection(request);
            assert(actual);
            const double nusselt = orientation == HorizontalPlateOrientation::HeatedFaceDownward
                ? 0.27 * std::pow(rayleigh, 0.25)
                : (rayleigh < 1.0e7
                    ? 0.54 * std::pow(rayleigh, 0.25)
                    : 0.15 * std::pow(rayleigh, 1.0 / 3.0));
            const double coefficient = nusselt * scenario.conductivity / characteristicLength;
            const double resistance = 1.0 / (coefficient * scenario.areaM2);
            const auto close = [](double lhs, double rhs) {
                return std::abs(lhs - rhs) <= 1.0e-10 * std::max(1.0, std::abs(rhs));
            };
            assert(close(actual.value().filmTemperatureK, film));
            assert(close(actual.value().rayleighNumber, rayleigh));
            assert(close(actual.value().nusseltNumber, nusselt));
            assert(close(actual.value().heatTransferCoefficientWPerM2K, coefficient));
            assert(close(actual.value().thermalResistanceKPerW, resistance));
            assert(actual.value().geometrySourceReference == request.geometrySourceReference);
            assert(actual.value().fluidPropertiesSourceReference == request.fluid.sourceReference);
            assert(actual.value().status == "PRELIMINARY");
        }
    }
}

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

    request.unobstructedQuiescentAmbient = true;
    request.orientation = static_cast<HorizontalPlateOrientation>(99);
    const auto invalidOrientation = analyzeHorizontalPlateNaturalConvection(request);
    assert(!invalidOrientation);

    request.orientation = HorizontalPlateOrientation::HeatedFaceUpward;
    request.exposedPerimeter = units::Length{1.0e8};
    const auto outOfRange = analyzeHorizontalPlateNaturalConvection(request);
    assert(!outOfRange);
}
 
int main()
{
    testHorizontalPlateCorrelations();
    testHorizontalPlateParameterMatrix();
    testVerticalPlateParameterMatrix();
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
