#include "p135/thermal/steady_state.hpp"

#include <cassert>
#include <cmath>
#include <limits>

using namespace p135;
using namespace p135::thermal;

namespace {

ThermalNodeInput validNode()
{
    ThermalNodeInput node;
    node.id = "CAP-1";
    node.label = "Filter capacitor";
    node.loss = units::Power{12.0};
    node.thermalResistanceKPerW = 1.5;
    node.maximumTemperature = units::TemperatureKelvin{358.15};
    node.lossSourceReference = "harmonic-loss-study:rev-1";
    node.thermalResistanceSourceReference = "manufacturer-thermal-data:rev-2";
    node.temperatureLimitSourceReference = "component-datasheet:rev-3";
    return node;
}

} // namespace

int main()
{
    {
        SteadyStateRequest request;
        request.ambientTemperature = units::TemperatureKelvin{298.15};
        request.nodes.push_back(validNode());
        const auto result = analyzeSteadyState(request);
        assert(result);
        const auto& node = result.value().nodes.front();
        assert(node.status == AssessmentStatus::Pass);
        assert(node.estimatedTemperature);
        assert(std::abs(node.estimatedTemperature->si() - 316.15) < 1.0e-9);
        assert(node.temperatureMargin);
        assert(std::abs(node.temperatureMargin->si() - 42.0) < 1.0e-9);
    }
    {
        SteadyStateRequest request;
        auto node = validNode();
        node.loss = units::Power{100.0};
        request.nodes.push_back(node);
        const auto result = analyzeSteadyState(request);
        assert(result);
        assert(result.value().nodes.front().status == AssessmentStatus::Fail);
        assert(result.value().nodes.front().temperatureMargin->si() < 0.0);
    }
    {
        SteadyStateRequest request;
        auto node = validNode();
        node.thermalResistanceKPerW.reset();
        request.nodes.push_back(node);
        const auto result = analyzeSteadyState(request);
        assert(result);
        assert(result.value().nodes.front().status == AssessmentStatus::NotVerified);
        assert(!result.value().nodes.front().estimatedTemperature);
    }
    {
        SteadyStateRequest request;
        auto node = validNode();
        node.thermalResistanceSourceReference.clear();
        request.nodes.push_back(node);
        const auto result = analyzeSteadyState(request);
        assert(result);
        assert(result.value().nodes.front().status == AssessmentStatus::NotVerified);
    }
    {
        SteadyStateRequest request;
        auto node = validNode();
        node.loss = units::Power{-1.0};
        request.nodes.push_back(node);
        assert(!analyzeSteadyState(request));
    }
    {
        SteadyStateRequest request;
        auto first = validNode();
        auto second = validNode();
        request.nodes = {first, second};
        assert(!analyzeSteadyState(request));
    }
    {
        SteadyStateRequest request;
        request.nodes.push_back(validNode());
        request.ambientTemperature = units::TemperatureKelvin{
            std::numeric_limits<double>::quiet_NaN()};
        assert(!analyzeSteadyState(request));
    }
    return 0;
}
