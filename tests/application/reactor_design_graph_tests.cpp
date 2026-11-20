#include "p135/application/reactor_design_graph.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

using namespace p135::application;

int main()
{
    const auto corePlan = planReactorRecalculation({"input.core.material"});
    assert(corePlan);
    const auto& core = corePlan.value().orderedStages;
    const auto position = [&core](const std::string& id) {
        const auto it = std::find(core.begin(), core.end(), id);
        assert(it != core.end());
        return static_cast<std::size_t>(std::distance(core.begin(), it));
    };
    assert(position("core.material") < position("core.magnetic_properties"));
    assert(position("core.magnetic_properties") < position("magnetic.operating_point"));
    assert(position("magnetic.operating_point") < position("core.losses"));
    assert(position("core.losses") < position("thermal.steady_state"));
    assert(position("thermal.steady_state") < position("design.verification"));
    assert(std::find(core.begin(), core.end(), "winding.resistance") == core.end());

    const auto windingPlan = planReactorRecalculation({"input.winding.conductor_material"});
    assert(windingPlan);
    const auto& winding = windingPlan.value().orderedStages;
    assert(std::find(winding.begin(), winding.end(), "winding.resistance") != winding.end());
    assert(std::find(winding.begin(), winding.end(), "winding.losses") != winding.end());
    assert(std::find(winding.begin(), winding.end(), "thermal.steady_state") != winding.end());
    assert(std::find(winding.begin(), winding.end(), "core.magnetic_properties") == winding.end());

    const auto thermalPlan = planReactorRecalculation({"input.thermal.ambient"});
    assert(thermalPlan);
    assert(thermalPlan.value().orderedStages.size() == 1);
    assert(thermalPlan.value().orderedStages[0] == "thermal.steady_state");

    const auto limitPlan = planReactorRecalculation(
        {"input.thermal.maximum_winding_temperature"});
    assert(limitPlan);
    assert(limitPlan.value().orderedStages.size() == 1);
    assert(limitPlan.value().orderedStages[0] == "design.verification");

    assert(!planReactorRecalculation({"unknown.input"}));
    const auto noChanges = planReactorRecalculation({});
    assert(noChanges && noChanges.value().orderedStages.empty());
    return 0;
}
