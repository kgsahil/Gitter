#pragma once

#include <string>
#include <utility>

namespace gitter {

enum class ErrorCode {
    None = 0,
    InvalidArgs,
    NotARepository,
    AlreadyInitialized,
    IoError,
    CorruptObject,
    RefNotFound,
    EmptyIndex,
    InternalError
};

struct Error {
    ErrorCode code{ErrorCode::None};
    std::string message;
};

template <typename T>
class Expected {
public:
    Expected(const T& value) : hasValue(true), value_(value) {}
    Expected(T&& value) : hasValue(true), value_(std::move(value)) {}
    Expected(const Error& err) : hasValue(false), error_(err) {}
    Expected(Error&& err) : hasValue(false), error_(std::move(err)) {}

    bool has_value() const { return hasValue; }
    explicit operator bool() const { return hasValue; }
    const T& value() const { return value_; }
    T& value() { return value_; }
    const Error& error() const { return error_; }

private:
    bool hasValue{false};
    T value_{};
    Error error_{};
};

template <>
class Expected<void> {
public:
    Expected() : ok(true) {}
    Expected(const Error& err) : ok(false), error_(err) {}
    Expected(Error&& err) : ok(false), error_(std::move(err)) {}
    bool has_value() const { return ok; }
    explicit operator bool() const { return ok; }
    const Error& error() const { return error_; }

private:
    bool ok{false};
    Error error_{};
};

}

 