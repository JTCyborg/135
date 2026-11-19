#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace p135::infrastructure::sqlite {
namespace {
Error codecError(const std::string& message, ErrorCode code)
{
    Error error;
    error.code = code;
    error.message = message;
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CANONICAL_JSON", message,
        std::nullopt, std::nullopt, std::nullopt
    });
    return error;
}
}

Result<std::string> canonicalizeJson(std::string_view input)
{
    if (input.empty())
        return Result<std::string>::failure(codecError(
            "JSON snapshot must not be empty.", ErrorCode::MissingRequiredData));
    try {
        const auto value = nlohmann::json::parse(input.begin(), input.end());
        return Result<std::string>::success(value.dump(
            -1, ' ', false, nlohmann::json::error_handler_t::strict));
    } catch (const nlohmann::json::exception& exception) {
        return Result<std::string>::failure(codecError(
            std::string("Invalid JSON snapshot: ") + exception.what(), ErrorCode::InvalidInput));
    }
}

Result<std::string> sha256Hex(std::string_view bytes)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &length,
                   EVP_sha256(), nullptr) != 1 || length != 32)
        return Result<std::string>::failure(codecError(
            "Unable to calculate SHA-256 digest.", ErrorCode::IoFailure));
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i)
        output << std::setw(2) << static_cast<unsigned int>(digest[i]);
    return Result<std::string>::success(output.str());
}

} // namespace p135::infrastructure::sqlite
