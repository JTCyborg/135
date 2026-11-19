#pragma once

#include <optional>
#include <string>
#include <vector>

namespace p135 {

enum class Severity { Info, Warning, Error };

struct Diagnostic {
    Severity severity{Severity::Info};
    std::string code;
    std::string message;
    std::optional<std::string> fieldPath;
    std::optional<double> measuredValueSI;
    std::optional<double> limitValueSI;
};

using Diagnostics = std::vector<Diagnostic>;

} // namespace p135
