#pragma once

#include "p135/core/result.hpp"

#include <string>
#include <vector>

namespace p135::application {

/// Stable IDs for the reactor design dependency graph. These identify model
/// stages, not claims that every stage is already implemented.
[[nodiscard]] const std::vector<std::string>& reactorDesignParameterIds();
[[nodiscard]] const std::vector<std::string>& reactorDesignDependencyPairs();

struct ReactorRecalculationPlan {
    std::vector<std::string> orderedStages;
};

/// Returns the dependency-first stages affected by changed construction inputs.
/// The graph describes required recalculation order; it does not execute solvers.
[[nodiscard]] Result<ReactorRecalculationPlan> planReactorRecalculation(
    const std::vector<std::string>& changedInputIds);

} // namespace p135::application
