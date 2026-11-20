#include "p135/thermal/winding_electrothermal.hpp"

#include <cassert>
#include <cmath>
#include <complex>

using namespace p135;
using namespace p135::thermal;

namespace {

WindingElectrothermalRequest request()
{
    WindingElectrothermalRequest input;
    auto& electrical = input.electrical;
    electrical.material.id = "CU-TEST";
    electrical.material.manufacturer = "Test";
    electrical.material.grade = "Copper";
    electrical.material.dataSource = domain::MaterialDataSource::ManufacturerDatasheet;
    electrical.material.sourceReference = "fixture:cu";
    electrical.material.datasetRevision = "R1";
    electrical.material.resistivityAt20C = units::Resistivity{1.707e-8};
    electrical.material.temperatureCoefficient = units::TemperatureCoefficient{0.00393};
    electrical.material.density = units::Density{8910.0};
    electrical.turns = 100;
    electrical.parallelConductors = 2;
    electrical.meanTurnLength = units::Length{0.5};
    electrical.bareConductorCrossSection = units::Area{1.0e-6};
    electrical.windingTemperature = units::TemperatureKelvin{293.15};
    electrical.harmonicCurrents = {
        {1, units::Frequency{50.0}, {
            {10.0, 0.0}, {-5.0, 8.660254037844386}, {-5.0, -8.660254037844386}}}
    };
    input.ambientTemperature = units::TemperatureKelvin{293.15};
    input.thermalResistanceKPerW = 0.5;
    input.maximumWindingTemperature = units::TemperatureKelvin{393.15};
    input.materialRevisionReference = "material-revision:CU-TEST-R1";
    input.thermalResistanceSourceReference = "thermal-test:rev-1";
    input.temperatureLimitSourceReference = "datasheet-test:rev-1";
    input.temperatureToleranceK = 1.0e-7;
    input.relaxationFactor = 0.5;
    input.maximumIterations = 200;
    return input;
}

} // namespace

int main()
{
    {
        const auto result = analyzeWindingElectrothermal(request());
        assert(result);
        assert(result.value().converged);
        assert(result.value().status == AssessmentStatus::Pass);
        assert(result.value().iterations > 1);
        assert(result.value().finalTemperatureResidualK <= 1.0e-7);
        assert(result.value().estimatedWindingTemperature.si() > 293.15);
        assert(result.value().copperLoss.totalThreePhaseCopperLoss.si() > 0.0);
    }
    {
        auto input = request();
        input.maximumIterations = 1;
        const auto result = analyzeWindingElectrothermal(input);
        assert(result);
        assert(!result.value().converged);
        assert(result.value().status == AssessmentStatus::NotVerified);
    }
    {
        auto input = request();
        input.maximumWindingTemperature = units::TemperatureKelvin{300.0};
        const auto result = analyzeWindingElectrothermal(input);
        assert(result);
        assert(result.value().converged);
        assert(result.value().status == AssessmentStatus::Fail);
    }
    {
        auto input = request();
        input.thermalResistanceKPerW = 0.0;
        assert(!analyzeWindingElectrothermal(input));
    }
    {
        auto input = request();
        input.materialRevisionReference.clear();
        assert(!analyzeWindingElectrothermal(input));
    }
    {
        auto input = request();
        input.relaxationFactor = 1.5;
        assert(!analyzeWindingElectrothermal(input));
    }
    return 0;
}
