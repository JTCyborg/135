#include "p135/application/reactor_design_execution.hpp"

#include "p135/domain/design_dependency.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace p135::application {
namespace {
Error executionError(ErrorCode code, std::string message)
{
    return {code, std::move(message), {}};
}
}

Result<ReactorDesignExecutionReport> executeReactorDesignPlan(
    const ReactorRecalculationPlan& plan,
    ReactorDesignExecutionState& state,
    const ReactorStageExecutor& executor)
{
    if (!executor)
        return Result<ReactorDesignExecutionReport>::failure(
            executionError(ErrorCode::InvalidInput, "A reactor stage executor is required."));
    if (plan.orderedStages.empty())
        return Result<ReactorDesignExecutionReport>::failure(
            executionError(ErrorCode::InvalidInput, "A recalculation plan must contain at least one stage."));

    std::vector<domain::DesignDependency> dependencies;
    dependencies.reserve(reactorDesignDependencyPairs().size());
    for (const auto& pair : reactorDesignDependencyPairs()) {
        const auto separator = pair.find('|');
        if (separator == std::string::npos)
            return Result<ReactorDesignExecutionReport>::failure(
                executionError(ErrorCode::InternalError, "Malformed reactor dependency graph."));
        dependencies.push_back({pair.substr(0, separator), pair.substr(separator + 1)});
    }
    const auto graphOrder = domain::designDependencyOrder(reactorDesignParameterIds(), dependencies);
    if (!graphOrder)
        return Result<ReactorDesignExecutionReport>::failure(graphOrder.error());

    std::unordered_set<std::string> seen;
    std::size_t previousPosition = 0;
    bool firstStage = true;
    for (const auto& stageId : plan.orderedStages) {
        const auto it = std::find(graphOrder.value().begin(), graphOrder.value().end(), stageId);
        if (stageId.empty() || stageId.starts_with("input.") ||
            it == graphOrder.value().end() || !seen.insert(stageId).second)
            return Result<ReactorDesignExecutionReport>::failure(
                executionError(ErrorCode::InvalidInput,
                    "Recalculation plan contains an input node, empty, unknown or duplicate stage ID."));
        const auto position = static_cast<std::size_t>(
            std::distance(graphOrder.value().begin(), it));
        if (!firstStage && position <= previousPosition)
            return Result<ReactorDesignExecutionReport>::failure(
                executionError(ErrorCode::InvalidInput,
                    "Recalculation plan violates dependency order."));
        firstStage = false;
        previousPosition = position;
    }

    state.candidateInProgress = true;
    state.failedStage.clear();
    state.candidate = state.accepted;
    state.candidate.number = state.accepted.number + 1;
    state.candidate.verified = false;

    for (const auto& stageId : plan.orderedStages)
        state.candidate.stages[stageId] = {ReactorStageStatus::NotVerified, {}, {}};

    ReactorDesignExecutionReport report;
    for (const auto& stageId : plan.orderedStages) {
        Result<std::string> result = Result<std::string>::failure(
            executionError(ErrorCode::InternalError, "Stage executor did not return a result."));
        try {
            result = executor(stageId);
        } catch (const std::exception& exception) {
            result = Result<std::string>::failure(
                executionError(ErrorCode::InternalError,
                    std::string("Stage executor threw an exception: ") + exception.what()));
        } catch (...) {
            result = Result<std::string>::failure(
                executionError(ErrorCode::InternalError,
                    "Stage executor threw a non-standard exception."));
        }
        report.executedStages.push_back(stageId);
        if (!result) {
            state.failedStage = stageId;
            state.candidateInProgress = false;
            report.failedStage = stageId;
            report.failureMessage = result.error().message;
            report.failureCode = result.error().code;
            report.committed = false;
            return Result<ReactorDesignExecutionReport>::success(std::move(report));
        }

        state.candidate.stages[stageId] = {
            ReactorStageStatus::Valid,
            std::to_string(state.candidate.number),
            std::move(result.value())
        };
    }

    state.candidate.verified = false;
    state.accepted = state.candidate;
    state.candidateInProgress = false;
    report.committed = true;
    return Result<ReactorDesignExecutionReport>::success(std::move(report));
}

} // namespace p135::application
