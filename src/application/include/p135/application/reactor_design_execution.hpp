#pragma once

#include "p135/application/reactor_design_graph.hpp"
#include "p135/core/result.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace p135::application {

enum class ReactorStageStatus { NotCalculated, Valid, NotVerified };

struct ReactorStageArtifact {
    ReactorStageStatus status{ReactorStageStatus::NotCalculated};
    std::string revisionId;
    std::string payload;
};

struct ReactorDesignRevision {
    std::uint64_t number{};
    bool verified{};
    std::map<std::string, ReactorStageArtifact> stages;
};

struct ReactorDesignExecutionState {
    // accepted is immutable during a candidate execution and survives failures.
    ReactorDesignRevision accepted;
    ReactorDesignRevision candidate;
    bool candidateInProgress{};
    std::string failedStage;
};

struct ReactorDesignExecutionReport {
    bool committed{};
    std::string failedStage;
    std::vector<std::string> executedStages;
};

/// A stage adapter returns an opaque serialized artifact. The application layer
/// deliberately does not interpret it or claim that a placeholder is physics.
using ReactorStageExecutor =
    std::function<Result<std::string>(const std::string& stageId)>;

/// Executes the affected stages transactionally. A failed stage prevents its
/// downstream stages from running; the previous accepted revision is retained.
/// The caller must provide real model adapters before a revision can be verified.
[[nodiscard]] Result<ReactorDesignExecutionReport> executeReactorDesignPlan(
    const ReactorRecalculationPlan& plan,
    ReactorDesignExecutionState& state,
    const ReactorStageExecutor& executor);

} // namespace p135::application
