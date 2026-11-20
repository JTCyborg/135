#pragma once

#include "p135/core/result.hpp"

#include <string>
#include <vector>

namespace p135::domain {

// A dependency edge is expressed as: parameterId depends on dependencyId.
// For example, winding.losses depends on winding.resistance and phase.current.
struct DesignDependency {
    std::string parameterId;
    std::string dependencyId;
};

// Returns a deterministic dependency-first ordering of all parameter IDs.
// Duplicate edges, empty IDs, and cycles are rejected.
[[nodiscard]] Result<std::vector<std::string>> designDependencyOrder(
    const std::vector<std::string>& parameterIds,
    const std::vector<DesignDependency>& dependencies);

// Returns changed IDs and every transitive dependent in dependency-first order.
// Unknown changed IDs, duplicate parameter IDs, malformed edges, and cycles fail.
[[nodiscard]] Result<std::vector<std::string>> affectedDesignParameters(
    const std::vector<std::string>& parameterIds,
    const std::vector<DesignDependency>& dependencies,
    const std::vector<std::string>& changedParameterIds);

} // namespace p135::domain
