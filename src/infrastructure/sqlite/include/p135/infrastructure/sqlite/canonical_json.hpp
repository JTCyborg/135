#pragma once

#include "p135/core/result.hpp"

#include <string>
#include <string_view>

namespace p135::infrastructure::sqlite {

/// Parse JSON and serialize it deterministically (sorted object keys, compact UTF-8).
[[nodiscard]] Result<std::string> canonicalizeJson(std::string_view input);

/// Lowercase hexadecimal SHA-256 digest of the exact supplied bytes.
[[nodiscard]] Result<std::string> sha256Hex(std::string_view bytes);

} // namespace p135::infrastructure::sqlite
