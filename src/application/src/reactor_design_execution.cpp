#include "p135/application/reactor_design_execution.hpp"

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

    state.candidateInProgress = true;
    state.failedStage.clear();
    state.candidate = state.accepted;
    state.candidate.number = state.accepted.number + 1;
    state.candidate.verified = false;

    // Invalidate every affected artifact up front. If execution stops halfway,
    // downstream outputs from the accepted revision cannot masquerade as current.
    for (const auto& stageId : plan.orderedStages) {
        state.candidate.stages[stageId] = {ReactorStageStatus::NotVerified, {}, {}};
    }

    ReactorDesignExecutionReport report;
    for (const auto& stageId : plan.orderedStages) {
        if (stageId.empty()) {
            state.candidateInProgress = false;
            state.failedStage = stageId;
            return Result<ReactorDesignExecutionReport>::failure(
                executionError(ErrorCode::InvalidInput, "A recalculation stage ID is empty."));
        }

        auto result = executor(stageId);
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

    // No physical certification is inferred from adapter success. Until a
    // dedicated verification model is connected, the revision stays unverified.
    state.candidate.verified = false;
    state.accepted = state.candidate;
    state.candidateInProgress = false;
    report.committed = true;
    return Result<ReactorDesignExecutionReport>::success(std::move(report));
}

} // namespace p135::application
