#include "p135/domain/winding_electrical.hpp"

#include <cassert>
#include <cmath>
#include <complex>
#include <limits>

using namespace p135;
using namespace p135::domain;

namespace {
ConductorMaterial copper()
{
    ConductorMaterial material;
    material.id = "TEST-CU";
    material.manufacturer = "Test source";
    material.grade = "Copper reference";
    material.dataSource = MaterialDataSource::ManufacturerDatasheet;
    material.sourceReference = "test-fixture";
    material.datasetRevision = "R1";
    material.resistivityAt20C = units::Resistivity{1.707e-8};
    material.temperatureCoefficient = units::TemperatureCoefficient{0.00393};
    material.density = units::Density{8910.0};
    return material;
}

WindingCopperLossRequest request()
{
    WindingCopperLossRequest input;
    input.material = copper();
    input.turns = 100;
    input.parallelConductors = 2;
    input.meanTurnLength = units::Length{0.5};
    input.bareConductorCrossSection = units::Area{1.0e-6};
    input.windingTemperature = units::TemperatureKelvin{373.15};
    input.harmonicCurrents = {
        {1, units::Frequency{50.0}, {
            std::complex<double>{10.0, 0.0},
            std::complex<double>{-5.0, 8.660254037844386},
            std::complex<double>{-5.0, -8.660254037844386}}},
        {5, units::Frequency{250.0}, {
            std::complex<double>{3.0, 0.0},
            std::complex<double>{-1.5, 2.598076211353316},
            std::complex<double>{-1.5, -2.598076211353316}}}
    };
    return input;
}
}

int main()
{
    const auto input = request();
    const auto result = calculateWindingCopperLoss(input);
    assert(result);

    const double rho20 = 1.707e-8;
    const double rhoOperating = rho20 * (1.0 + 0.00393 * 80.0);
    const double expectedResistance = rhoOperating * 100.0 * 0.5 / (2.0e-6);
    assert(std::abs(result.value().resistanceAt20C.si() - 0.42675) < 1.0e-10);
    assert(std::abs(result.value().resistivityAtOperatingTemperature.si() -
                    rhoOperating) < 1.0e-18);
    assert(std::abs(result.value().resistanceAtOperatingTemperature.si() -
                    expectedResistance) < 1.0e-10);
    assert(result.value().harmonicLosses.size() == 2);
    assert(std::abs(result.value().harmonicLosses[0].totalThreePhaseLoss.si() -
                    300.0 * expectedResistance) < 1.0e-8);
    assert(std::abs(result.value().harmonicLosses[1].totalThreePhaseLoss.si() -
                    27.0 * expectedResistance) < 1.0e-8);
    assert(std::abs(result.value().totalThreePhaseCopperLoss.si() -
                    327.0 * expectedResistance) < 1.0e-8);
    assert(!result.value().includesAcEffects);

    auto bad = request();
    bad.parallelConductors = 0;
    assert(!calculateWindingCopperLoss(bad));

    bad = request();
    bad.harmonicCurrents[1].order = 1;
    const auto duplicate = calculateWindingCopperLoss(bad);
    assert(!duplicate && duplicate.error().code == ErrorCode::InvalidInput);

    bad = request();
    bad.harmonicCurrents[0].phaseRmsCurrent[0] = {
        std::numeric_limits<double>::quiet_NaN(), 0.0};
    assert(!calculateWindingCopperLoss(bad));

    bad = request();
    bad.harmonicCurrents.clear();
    const auto missing = calculateWindingCopperLoss(bad);
    assert(!missing && missing.error().code == ErrorCode::MissingRequiredData);

    bad = request();
    bad.windingTemperature = units::TemperatureKelvin{-1.0};
    assert(!calculateWindingCopperLoss(bad));
    return 0;
}
