#include "p135/thermal/thermal_network.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

using namespace p135;
using namespace p135::thermal;

namespace {

ThermalNetworkNodeInput node(std::string id, double loss, double limit)
{
    ThermalNetworkNodeInput value;
    value.id = std::move(id);
    value.label = value.id;
    value.loss = units::Power{loss};
    value.maximumTemperature = units::TemperatureKelvin{limit};
    value.lossSourceReference = "loss-study:rev-1";
    value.temperatureLimitSourceReference = "datasheet:rev-1";
    return value;
}

} // namespace

int main()
{
    {
        // Node A: 10 W, G(A-ambient)=1 W/K, G(A-B)=1 W/K.
        // Node B: 0 W, G(B-ambient)=1 W/K.
        // Exact solution: A=299.8167 K, B=296.4833 K at 293.15 K ambient.
        ThermalNetworkRequest request;
        request.nodes = {node("A", 10.0, 330.0), node("B", 0.0, 320.0)};
        request.links.push_back({"A", "B", 1.0, "thermal-model:rev-1"});
        request.ambientLinks.push_back({"A", 1.0, "cooling-model:rev-1"});
        request.ambientLinks.push_back({"B", 1.0, "cooling-model:rev-1"});
        const auto result = analyzeThermalNetwork(request);
        assert(result);
        assert(result.value().solved);
        assert(result.value().modelProvenanceComplete);
        assert(result.value().nodes.size() == 2);
        assert(std::abs(result.value().nodes[0].estimatedTemperature.si() - (293.15 + 20.0 / 3.0)) < 1.0e-9);
        assert(std::abs(result.value().nodes[1].estimatedTemperature.si() - (293.15 + 10.0 / 3.0)) < 1.0e-9);
        assert(result.value().nodes[0].status == AssessmentStatus::Pass);
        assert(result.value().nodes[1].status == AssessmentStatus::Pass);
    }
    {
        ThermalNetworkRequest request;
        request.nodes = {node("A", 10.0, 300.0)};
        request.ambientLinks.push_back({"A", 1.0, "cooling-model:rev-1"});
        const auto result = analyzeThermalNetwork(request);
        assert(result);
        assert(result.value().nodes.front().status == AssessmentStatus::Fail);
        assert(result.value().nodes.front().temperatureMargin->si() < 0.0);
    }
    {
        ThermalNetworkRequest request;
        request.nodes = {node("A", 10.0, 330.0), node("B", 0.0, 320.0)};
        request.links.push_back({"A", "B", 1.0, "thermal-model:rev-1"});
        request.ambientLinks.push_back({"A", 1.0, "cooling-model:rev-1"});
        // Both nodes are in the same connected component and A is grounded to ambient.
        const auto result = analyzeThermalNetwork(request);
        assert(result);
        assert(std::abs(result.value().nodes[0].estimatedTemperature.si() - 303.15) < 1.0e-9);
        assert(std::abs(result.value().nodes[1].estimatedTemperature.si() - 303.15) < 1.0e-9);
    }
    {
        ThermalNetworkRequest request;
        request.nodes = {node("A", 1.0, 330.0), node("B", 1.0, 330.0)};
        request.ambientLinks.push_back({"A", 1.0, "cooling-model:rev-1"});
        assert(!analyzeThermalNetwork(request)); // B is a floating component.
    }
    {
        ThermalNetworkRequest request;
        request.nodes = {node("A", 1.0, 330.0)};
        request.ambientLinks.push_back({"A", 0.0, "cooling-model:rev-1"});
        assert(!analyzeThermalNetwork(request));
    }
    {
        ThermalNetworkRequest request;
        request.nodes = {node("A", 1.0, 330.0)};
        request.ambientLinks.push_back({"A", 1.0, ""});
        const auto result = analyzeThermalNetwork(request);
        assert(result);
        assert(!result.value().modelProvenanceComplete);
        assert(result.value().nodes.front().status == AssessmentStatus::NotVerified);
    }
    {
        ThermalNetworkRequest request;
        request.nodes = {node("A", 1.0, 330.0)};
        request.ambientLinks.push_back({"UNKNOWN", 1.0, "cooling-model:rev-1"});
        assert(!analyzeThermalNetwork(request));
    }
    {
        ThermalNetworkRequest request;
        request.nodes = {node("A", std::numeric_limits<double>::quiet_NaN(), 330.0)};
        request.ambientLinks.push_back({"A", 1.0, "cooling-model:rev-1"});
        assert(!analyzeThermalNetwork(request));
    }
    return 0;
}
