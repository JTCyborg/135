#include "p135/application/reactor_design_graph.hpp"

#include "p135/domain/design_dependency.hpp"

#include <algorithm>

namespace p135::application {

const std::vector<std::string>& reactorDesignParameterIds()
{
    static const std::vector<std::string> ids{
        "input.filter.excitation",
        "input.core.material",
        "input.core.geometry",
        "input.core.stacking_factor",
        "input.winding.conductor_material",
        "input.winding.conductor_geometry",
        "input.winding.turns",
        "input.winding.insulation",
        "input.winding.technology",
        "input.winding.bobbin",
        "input.thermal.cooling",
        "input.thermal.ambient",
        "input.thermal.maximum_winding_temperature",
        "filter.excitation",
        "core.material",
        "core.geometry",
        "core.stacking_factor",
        "core.magnetic_properties",
        "magnetic.operating_point",
        "winding.conductor_material",
        "winding.conductor_geometry",
        "winding.turns",
        "winding.insulation",
        "winding.geometry",
        "winding.resistance",
        "winding.losses",
        "core.losses",
        "thermal.steady_state",
        "assembly.geometry",
        "design.verification"
    };
    return ids;
}

const std::vector<std::string>& reactorDesignDependencyPairs()
{
    // Encoded as "dependent|dependency" to keep the public graph representation
    // simple while preserving the domain layer's explicit edge semantics.
    static const std::vector<std::string> pairs{
        "core.material|input.core.material",
        "core.geometry|input.core.geometry",
        "core.stacking_factor|input.core.stacking_factor",
        "winding.conductor_material|input.winding.conductor_material",
        "winding.conductor_geometry|input.winding.conductor_geometry",
        "winding.turns|input.winding.turns",
        "winding.insulation|input.winding.insulation",
        "winding.geometry|input.winding.technology",
        "winding.geometry|input.winding.bobbin",
        "thermal.cooling|input.thermal.cooling",
        "thermal.ambient|input.thermal.ambient",
        "design.verification|input.thermal.maximum_winding_temperature",
        "filter.excitation|input.filter.excitation",
        "core.magnetic_properties|core.material",
        "core.magnetic_properties|core.geometry",
        "core.magnetic_properties|core.stacking_factor",
        "magnetic.operating_point|filter.excitation",
        "magnetic.operating_point|core.magnetic_properties",
        "magnetic.operating_point|winding.turns",
        "winding.geometry|winding.conductor_geometry",
        "winding.geometry|winding.insulation",
        "winding.geometry|winding.turns",
        "winding.resistance|winding.conductor_material",
        "winding.resistance|winding.conductor_geometry",
        "winding.resistance|winding.turns",
        "winding.losses|winding.resistance",
        "winding.losses|filter.excitation",
        "core.losses|core.material",
        "core.losses|core.geometry",
        "core.losses|magnetic.operating_point",
        "thermal.steady_state|winding.losses",
        "thermal.steady_state|core.losses",
        "thermal.steady_state|input.thermal.cooling",
        "thermal.steady_state|input.thermal.ambient",
        "assembly.geometry|core.geometry",
        "assembly.geometry|winding.geometry",
        "design.verification|magnetic.operating_point",
        "design.verification|winding.geometry",
        "design.verification|thermal.steady_state",
        "design.verification|assembly.geometry"
    };
    return pairs;
}

Result<ReactorRecalculationPlan> planReactorRecalculation(
    const std::vector<std::string>& changedInputIds)
{
    std::vector<domain::DesignDependency> edges;
    edges.reserve(reactorDesignDependencyPairs().size());
    for (const auto& pair : reactorDesignDependencyPairs()) {
        const auto separator = pair.find('|');
        if (separator == std::string::npos)
            return Result<ReactorRecalculationPlan>::failure(
                {ErrorCode::InvalidInput, "Malformed reactor dependency edge.", {}});
        edges.push_back({pair.substr(0, separator), pair.substr(separator + 1)});
    }
    auto affected = domain::affectedDesignParameters(
        reactorDesignParameterIds(), edges, changedInputIds);
    if (!affected)
        return Result<ReactorRecalculationPlan>::failure(affected.error());

    // Input nodes seed invalidation but are not executable calculation stages.
    auto stages = std::move(affected.value());
    stages.erase(std::remove_if(stages.begin(), stages.end(),
        [](const std::string& id) { return id.starts_with("input."); }), stages.end());
    return Result<ReactorRecalculationPlan>::success({std::move(stages)});
}

} // namespace p135::application
