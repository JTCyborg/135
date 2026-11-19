#pragma once

#include "p135/core/diagnostic.hpp"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace p135 {

enum class ErrorCode {
    InvalidInput,
    MissingRequiredData,
    MissingMaterialProperty,
    UnsupportedConfiguration,
    ModelOutOfDomain,
    SingularSystem,
    NonConvergence,
    SolverFailure,
    Cancelled,
    IoFailure,
    InternalError
};

struct Error {
    ErrorCode code{ErrorCode::InternalError};
    std::string message;
    Diagnostics diagnostics;
};

template <class T>
class [[nodiscard]] Result {
public:
    static Result success(T value, Diagnostics diagnostics = {}) {
        return Result(std::move(value), std::move(diagnostics));
    }

    static Result failure(Error error) {
        return Result(std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    explicit operator bool() const noexcept { return hasValue(); }

    [[nodiscard]] const T& value() const& { return std::get<T>(storage_); }
    [[nodiscard]] T& value() & { return std::get<T>(storage_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(storage_)); }
    [[nodiscard]] const Error& error() const& { return std::get<Error>(storage_); }

    [[nodiscard]] const Diagnostics& diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    explicit Result(T value, Diagnostics diagnostics)
        : storage_(std::move(value)), diagnostics_(std::move(diagnostics)) {}
    explicit Result(Error error) : storage_(std::move(error)) {}

    std::variant<T, Error> storage_;
    Diagnostics diagnostics_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    static Result success(Diagnostics diagnostics = {}) {
        return Result(true, {}, std::move(diagnostics));
    }

    static Result failure(Error error) {
        return Result(false, std::move(error), {});
    }

    [[nodiscard]] bool hasValue() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }
    [[nodiscard]] const Error& error() const& { return error_; }
    [[nodiscard]] const Diagnostics& diagnostics() const noexcept {
        return diagnostics_;
    }

private:
    Result(bool ok, Error error, Diagnostics diagnostics)
        : ok_(ok), error_(std::move(error)), diagnostics_(std::move(diagnostics)) {}

    bool ok_{false};
    Error error_{};
    Diagnostics diagnostics_;
};

} // namespace p135
