#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <optional>
#include <utility>

namespace workbench::core {

/**
 * @brief Standard status codes for workbench engineering operations.
 */
enum class StatusCode {
    Ok = 0,
    InvalidInput,
    OutOfRange,
    DegenerateGeometry,
    FileNotFound,
    ComputationError,
    Cancelled,
    DeviceNotFound,
    ConnectionFailed,
    Timeout,
    UnsupportedOperation,
    DeviceDisconnected
};

/**
 * @brief Structured error reporting type holding an error code and explanatory message.
 */
struct Error {
    StatusCode code;
    std::string message;

    Error(StatusCode c, std::string msg)
        : code(c), message(std::move(msg)) {}
};

/**
 * @brief Type-safe result holder containing either a success value of type T or an Error.
 * 
 * Provides robust, exception-safe error handling for engineering algorithms.
 */
template <typename T>
class Result {
public:
    static Result<T> success(T value) {
        return Result<T>(std::move(value));
    }

    static Result<T> failure(StatusCode code, std::string message) {
        return Result<T>(Error(code, std::move(message)));
    }

    static Result<T> failure(Error err) {
        return Result<T>(std::move(err));
    }

    bool is_ok() const noexcept {
        return std::holds_alternative<T>(m_data);
    }

    bool is_error() const noexcept {
        return std::holds_alternative<Error>(m_data);
    }

    const T& value() const {
        return std::get<T>(m_data);
    }

    T& value() {
        return std::get<T>(m_data);
    }

    const Error& error() const {
        return std::get<Error>(m_data);
    }

private:
    explicit Result(T value) : m_data(std::move(value)) {}
    explicit Result(Error err) : m_data(std::move(err)) {}

    std::variant<T, Error> m_data;
};

/**
 * @brief Status indicator for operations that do not return a data value.
 */
class Status {
public:
    static Status ok() {
        return Status();
    }

    static Status failure(StatusCode code, std::string message) {
        return Status(Error(code, std::move(message)));
    }

    bool is_ok() const noexcept {
        return !m_error.has_value();
    }

    bool is_error() const noexcept {
        return m_error.has_value();
    }

    const Error& error() const {
        return m_error.value();
    }

private:
    Status() = default;
    explicit Status(Error err) : m_error(std::move(err)) {}

    std::optional<Error> m_error;
};

} // namespace workbench::core
