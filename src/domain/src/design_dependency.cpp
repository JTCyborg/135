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
    std::unordered_map<std::string, std::size_t> dependencyCount;
    std::unordered_set<std::string> edges;
    for (const auto& id : parameterIds)
        dependencyCount.emplace(id, 0U);

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
        ++dependencyCount[edge.parameterId];
    }

    for (auto& [id, values] : dependents) {
        (void)id;
        std::sort(values.begin(), values.end());
    }

    std::vector<std::string> ready;
    for (const auto& [id, count] : dependencyCount) {
        if (count == 0)
            ready.push_back(id);
    }
    std::sort(ready.begin(), ready.end());

    std::vector<std::string> order;
    order.reserve(parameterIds.size());
    while (!ready.empty()) {
        const auto id = ready.front();
        ready.erase(ready.begin());
        order.push_back(id);
        const auto it = dependents.find(id);
        if (it == dependents.end())
            continue;
        for (const auto& dependent : it->second) {
            auto& count = dependencyCount[dependent];
            if (--count == 0) {
                const auto position = std::lower_bound(ready.begin(), ready.end(), dependent);
                ready.insert(position, dependent);
            }
        }
    }

    if (order.size() != parameterIds.size())
        return failure("Design dependency graph contains a cycle.");
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
