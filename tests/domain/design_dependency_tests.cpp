#include "p135/domain/design_dependency.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace p135::domain;

int main()
{
    const std::vector<std::string> ids{
        "filter.inductance", "core.flux", "core.geometry",
        "winding.turns", "winding.losses", "thermal.temperature"
    };
    const std::vector<DesignDependency> edges{
        {"core.flux", "core.geometry"},
        {"winding.turns", "core.flux"},
        {"filter.inductance", "winding.turns"},
        {"winding.losses", "winding.turns"},
        {"thermal.temperature", "winding.losses"}
    };

    const auto order = designDependencyOrder(ids, edges);
    assert(order);
    const auto position = [&](const std::string& id) {
        for (std::size_t i = 0; i < order.value().size(); ++i)
            if (order.value()[i] == id)
                return i;
        return order.value().size();
    };
    assert(position("core.geometry") < position("core.flux"));
    assert(position("core.flux") < position("winding.turns"));
    assert(position("winding.turns") < position("winding.losses"));
    assert(position("winding.losses") < position("thermal.temperature"));

    const auto affected = affectedDesignParameters(ids, edges, {"core.geometry"});
    assert(affected);
    assert(affected.value().size() == 6);
    assert(affected.value().front() == "core.geometry");
    assert(affected.value().back() == "thermal.temperature");

    const auto partial = affectedDesignParameters(ids, edges, {"winding.turns"});
    assert(partial);
    assert(partial.value().size() == 4);
    assert(partial.value()[0] == "winding.turns");
    assert(partial.value()[1] == "filter.inductance" ||
           partial.value()[1] == "winding.losses");

    assert(!affectedDesignParameters(ids, edges, {"missing.parameter"}));
    assert(!designDependencyOrder({"a", "a"}, {}));
    assert(!designDependencyOrder({"a"}, {{"a", "missing"}}));
    assert(!designDependencyOrder({"a", "b"}, {{"a", "b"}, {"b", "a"}}));
    assert(!designDependencyOrder({"a"}, {{"a", "a"}}));
    assert(!designDependencyOrder({"a"}, {{"a", "a"}, {"a", "a"}}));
    return 0;
}
