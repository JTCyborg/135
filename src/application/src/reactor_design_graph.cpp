#include "p135/application/reactor_design_graph.hpp"

#include "p135/domain/design_dependency.hpp"

namespace p135::application {

const std::vector<std::string>& reactorDesignParameterIds()
{
    static const std::vector<std::string> ids{
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
        "thermal.cooling",
        "thermal.ambient",
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
        "thermal.steady_state|thermal.cooling",
        "thermal.steady_state|thermal.ambient",
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
    return Result<ReactorRecalculationPlan>::success({std::move(affected.value())});
}

} // namespace p135::application
