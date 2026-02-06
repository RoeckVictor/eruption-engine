#pragma once

#include "engine/core/Error.h"
#include <stdexcept>
#include <variant>
#include <utility>

namespace engine {

/// Result type for error handling (similar to Rust's Result or C++23's std::expected).
/// Holds either a success value of type T or an error of type E.
///
/// Usage:
///   Result<int, EngineError> divide(int a, int b) {
///       if (b == 0) return Err(EngineError::InvalidArgument);
///       return Ok(a / b);
///   }
///
///   auto result = divide(10, 2);
///   if (result.is_ok()) {
///       int value = result.value();
///   } else {
///       EngineError error = result.error();
///   }
template<typename T, typename E = ErrorInfo>
class Result {
public:
    /// Construct a successful result.
    static Result Ok(T value) {
        return Result(std::move(value));
    }

    /// Construct an error result.
    static Result Err(E error) {
        return Result(std::move(error));
    }

    /// Default constructor creates an error result.
    Result() : m_data(E{}) {}

    /// Construct from value (success).
    Result(T value) : m_data(std::move(value)) {}

    /// Construct from error.
    Result(E error) : m_data(std::move(error)) {}

    /// Check if the result is successful.
    bool is_ok() const {
        return std::holds_alternative<T>(m_data);
    }

    /// Check if the result is an error.
    bool is_err() const {
        return std::holds_alternative<E>(m_data);
    }

    /// Get the success value (throws if error).
    T& value() & {
        return std::get<T>(m_data);
    }

    /// Get the success value (throws if error).
    const T& value() const & {
        return std::get<T>(m_data);
    }

    /// Get the success value (throws if error).
    T&& value() && {
        return std::get<T>(std::move(m_data));
    }

    /// Get the error (throws if success).
    E& error() & {
        return std::get<E>(m_data);
    }

    /// Get the error (throws if success).
    const E& error() const & {
        return std::get<E>(m_data);
    }

    /// Get the success value or a default value.
    T value_or(T default_value) const & {
        if (is_ok()) {
            return value();
        }
        return default_value;
    }

    /// Get the success value or a default value.
    T value_or(T default_value) && {
        if (is_ok()) {
            return std::move(value());
        }
        return default_value;
    }

    /// Unwrap the value (for use when you know it's Ok).
    /// Throws std::runtime_error if called on an Err value.
    T& unwrap() & {
        if (is_err()) {
            throw std::runtime_error("Called unwrap() on an Err value");
        }
        return value();
    }

    /// Unwrap the value (for use when you know it's Ok).
    /// Throws std::runtime_error if called on an Err value.
    T&& unwrap() && {
        if (is_err()) {
            throw std::runtime_error("Called unwrap() on an Err value");
        }
        return std::move(value());
    }

    /// Conversion to bool (true if Ok, false if Err).
    explicit operator bool() const {
        return is_ok();
    }

private:
    std::variant<T, E> m_data;
};

/// Specialization for void results (no value on success, only error or success).
template<typename E>
class Result<void, E> {
public:
    /// Construct a successful result.
    static Result Ok() {
        return Result(true);
    }

    /// Construct an error result.
    static Result Err(E error) {
        return Result(std::move(error));
    }

    /// Default constructor creates a success result.
    Result() : m_is_ok(true), m_error() {}

    /// Construct success.
    explicit Result(bool ok) : m_is_ok(ok), m_error() {}

    /// Construct from error.
    Result(E error) : m_is_ok(false), m_error(std::move(error)) {}

    /// Check if the result is successful.
    bool is_ok() const {
        return m_is_ok;
    }

    /// Check if the result is an error.
    bool is_err() const {
        return !m_is_ok;
    }

    /// Get the error (throws if success).
    E& error() & {
        return m_error;
    }

    /// Get the error (throws if success).
    const E& error() const & {
        return m_error;
    }

    /// Conversion to bool (true if Ok, false if Err).
    explicit operator bool() const {
        return is_ok();
    }

private:
    bool m_is_ok;
    E m_error;
};

/// Convenience function to create Ok results.
template<typename T>
Result<T, ErrorInfo> Ok(T value) {
    return Result<T, ErrorInfo>::Ok(std::move(value));
}

/// Convenience function to create Ok results for void.
inline Result<void, ErrorInfo> Ok() {
    return Result<void, ErrorInfo>::Ok();
}

/// Convenience function to create Err results with EngineError.
template<typename T>
Result<T, ErrorInfo> Err(EngineError error_code) {
    return Result<T, ErrorInfo>::Err(ErrorInfo(error_code));
}

/// Convenience function to create Err results with message.
template<typename T>
Result<T, ErrorInfo> Err(EngineError error_code, std::string message) {
    return Result<T, ErrorInfo>::Err(ErrorInfo(error_code, std::move(message)));
}

/// Convenience function to create Err results with message and context.
template<typename T>
Result<T, ErrorInfo> Err(EngineError error_code, std::string message, std::string context) {
    return Result<T, ErrorInfo>::Err(ErrorInfo(error_code, std::move(message), std::move(context)));
}

/// Convenience function to create void Err results.
inline Result<void, ErrorInfo> Err(EngineError error_code) {
    return Result<void, ErrorInfo>::Err(ErrorInfo(error_code));
}

/// Convenience function to create void Err results with message.
inline Result<void, ErrorInfo> Err(EngineError error_code, std::string message) {
    return Result<void, ErrorInfo>::Err(ErrorInfo(error_code, std::move(message)));
}

/// Convenience function to create void Err results with message and context.
inline Result<void, ErrorInfo> Err(EngineError error_code, std::string message, std::string context) {
    return Result<void, ErrorInfo>::Err(ErrorInfo(error_code, std::move(message), std::move(context)));
}

} // namespace engine
