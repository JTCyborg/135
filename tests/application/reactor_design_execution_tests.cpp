#include "p135/application/reactor_design_execution.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace p135;
using namespace p135::application;

int main()
{
    ReactorDesignExecutionState state;
    state.accepted.number = 7;
    state.accepted.verified = true;
    state.accepted.stages["core.material"] = {
        ReactorStageStatus::Valid, "7", "previous-core"
    };

    ReactorRecalculationPlan plan{{"core.material", "core.magnetic_properties",
                                   "magnetic.operating_point"}};
    std::vector<std::string> calls;
    const auto failed = executeReactorDesignPlan(plan, state,
        [&calls](const std::string& id) -> Result<std::string> {
            calls.push_back(id);
            if (id == "core.magnetic_properties")
                return Result<std::string>::failure(
                    {ErrorCode::NonConvergence, "magnetic solver did not converge", {}});
            return Result<std::string>::success("artifact:" + id);
        });

    assert(failed);
    assert(!failed.value().committed);
    assert(failed.value().failedStage == "core.magnetic_properties");
    assert(calls.size() == 2);
    assert(state.accepted.number == 7);
    assert(state.accepted.verified);
    assert(state.accepted.stages.at("core.material").payload == "previous-core");
    assert(state.candidate.number == 8);
    assert(!state.candidate.verified);
    assert(state.candidate.stages.at("core.magnetic_properties").status ==
           ReactorStageStatus::NotVerified);
    assert(state.candidate.stages.at("magnetic.operating_point").status ==
           ReactorStageStatus::NotVerified);
    assert(failed.value().failureCode == ErrorCode::NonConvergence);
    assert(failed.value().failureMessage == "magnetic solver did not converge");
    assert(!state.candidateInProgress);

    const auto success = executeReactorDesignPlan(
        ReactorRecalculationPlan{{"core.material"}},
        state,
        [](const std::string& id) {
            return Result<std::string>::success("new:" + id);
        });
    assert(success && success.value().committed);
    assert(state.accepted.number == 8);
    assert(!state.accepted.verified);
    assert(state.accepted.stages.at("core.material").payload == "new:core.material");

    const auto reversed = executeReactorDesignPlan(
        ReactorRecalculationPlan{{"magnetic.operating_point", "core.magnetic_properties"}},
        state,
        [](const std::string& id) { return Result<std::string>::success(id); });
    assert(!reversed);
    assert(reversed.error().code == ErrorCode::InvalidInput);

    const auto duplicate = executeReactorDesignPlan(
        ReactorRecalculationPlan{{"core.material", "core.material"}},
        state,
        [](const std::string& id) { return Result<std::string>::success(id); });
    assert(!duplicate);

    bool inputNodeExecuted = false;
    const auto inputNode = executeReactorDesignPlan(
        ReactorRecalculationPlan{{"input.core.material"}},
        state,
        [&inputNodeExecuted](const std::string&) {
            inputNodeExecuted = true;
            return Result<std::string>::success("should-not-run");
        });
    assert(!inputNode);
    assert(!inputNodeExecuted);

    const auto noExecutor = executeReactorDesignPlan(plan, state, {});
    assert(!noExecutor);

    const auto emptyPlan = executeReactorDesignPlan(
        ReactorRecalculationPlan{}, state,
        [](const std::string& id) { return Result<std::string>::success(id); });
    assert(!emptyPlan);
    assert(emptyPlan.error().code == ErrorCode::InvalidInput);

    const auto revisionBeforeThrow = state.accepted.number;
    const auto throwing = executeReactorDesignPlan(
        ReactorRecalculationPlan{{"core.material"}}, state,
        [](const std::string&) -> Result<std::string> {
            throw std::runtime_error("simulated adapter failure");
        });
    assert(throwing);
    assert(!throwing.value().committed);
    assert(throwing.value().failedStage == "core.material");
    assert(throwing.value().failureCode == ErrorCode::InternalError);
    assert(throwing.value().failureMessage.find("simulated adapter failure") != std::string::npos);
    assert(state.accepted.number == revisionBeforeThrow);
    assert(!state.candidateInProgress);
    assert(state.failedStage == "core.material");

    const auto nonStandardThrow = executeReactorDesignPlan(
        ReactorRecalculationPlan{{"core.material"}}, state,
        [](const std::string&) -> Result<std::string> { throw 42; });
    assert(nonStandardThrow);
    assert(!nonStandardThrow.value().committed);
    assert(nonStandardThrow.value().failureCode == ErrorCode::InternalError);
    assert(!state.candidateInProgress);
    return 0;
}
