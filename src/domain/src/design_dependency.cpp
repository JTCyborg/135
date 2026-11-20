#include "p135/domain/design_dependency.hpp"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace p135::domain {
namespace {
Result<std::vector<std::string>> failure(std::string message)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    return Result<std::vector<std::string>>::failure(std::move(error));
}
}

Result<std::vector<std::string>> designDependencyOrder(
    const std::vector<std::string>& parameterIds,
    const std::vector<DesignDependency>& dependencies)
{
    std::unordered_set<std::string> known;
    known.reserve(parameterIds.size());
    for (const auto& id : parameterIds) {
        if (id.empty())
            return failure("Design parameter identifiers must not be empty.");
        if (!known.insert(id).second)
            return failure("Duplicate design parameter identifier: " + id);
    }

    std::unordered_map<std::string, std::vector<std::string>> dependents;
    std::unordered_set<std::string> edges;
    for (const auto& edge : dependencies) {
        if (edge.parameterId.empty() || edge.dependencyId.empty())
            return failure("Design dependency identifiers must not be empty.");
        if (!known.contains(edge.parameterId) || !known.contains(edge.dependencyId))
            return failure("Design dependency references an unknown parameter.");
        if (edge.parameterId == edge.dependencyId)
            return failure("A design parameter cannot depend on itself: " + edge.parameterId);
        const std::string key = edge.dependencyId + "\n" + edge.parameterId;
        if (!edges.insert(key).second)
            return failure("Duplicate design dependency edge.");
        dependents[edge.dependencyId].push_back(edge.parameterId);
    }

    for (auto& [id, values] : dependents) {
        (void)id;
        std::sort(values.begin(), values.end());
    }

    enum class Visit { Unseen, Active, Complete };
    std::unordered_map<std::string, Visit> visits;
    std::vector<std::string> order;
    order.reserve(parameterIds.size());

    std::function<bool(const std::string&)> visit = [&](const std::string& id) {
        const auto state = visits.contains(id) ? visits.at(id) : Visit::Unseen;
        if (state == Visit::Active)
            return false;
        if (state == Visit::Complete)
            return true;
        visits[id] = Visit::Active;
        auto children = dependents.find(id);
        if (children != dependents.end()) {
            for (const auto& child : children->second) {
                if (!visit(child))
                    return false;
            }
        }
        visits[id] = Visit::Complete;
        order.push_back(id);
        return true;
    };

    // Visit roots in sorted order so the result is stable across platforms.
    auto roots = parameterIds;
    std::sort(roots.begin(), roots.end());
    for (const auto& id : roots) {
        if (!visit(id))
            return failure("Design dependency graph contains a cycle.");
    }
    return Result<std::vector<std::string>>::success(std::move(order));
}

Result<std::vector<std::string>> affectedDesignParameters(
    const std::vector<std::string>& parameterIds,
    const std::vector<DesignDependency>& dependencies,
    const std::vector<std::string>& changedParameterIds)
{
    auto orderResult = designDependencyOrder(parameterIds, dependencies);
    if (!orderResult)
        return failure(orderResult.error().message);

    std::unordered_set<std::string> affected;
    for (const auto& id : changedParameterIds) {
        if (std::find(parameterIds.begin(), parameterIds.end(), id) == parameterIds.end())
            return failure("Changed parameter is not part of the design: " + id);
        affected.insert(id);
    }

    std::unordered_map<std::string, std::vector<std::string>> dependents;
    for (const auto& edge : dependencies)
        dependents[edge.dependencyId].push_back(edge.parameterId);

    std::vector<std::string> pending(affected.begin(), affected.end());
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto it = dependents.find(pending[i]);
        if (it == dependents.end())
            continue;
        for (const auto& dependent : it->second) {
            if (affected.insert(dependent).second)
                pending.push_back(dependent);
        }
    }

    std::vector<std::string> result;
    for (const auto& id : orderResult.value()) {
        if (affected.contains(id))
            result.push_back(id);
    }
    return Result<std::vector<std::string>>::success(std::move(result));
}

} // namespace p135::domain
