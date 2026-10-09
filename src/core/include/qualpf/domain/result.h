#pragma once

#include <string>
#include <utility>
#include <variant>

namespace qualpf {

enum class ErrorCode {
    invalid_input,
    missing_data,
    unsupported,
    stale_snapshot,
    closed_edit,
    exhausted_revision,
    cancelled,
    rank_deficient,
    numerical_failure,
    undefined_preprocessing
};

struct Error {
    ErrorCode code;
    std::string field;   // Stable field path, not translated UI text.
    std::string message;
    std::string reason; // Stable optional machine reason; never inferred from
                        // translated text.
};

// No default construction: absent/failed computation cannot look like a zero score.
template<class T>
class [[nodiscard]] Result {
public:
    explicit Result(T value) : storage_(std::move(value)) {}
    explicit Result(Error error) : storage_(std::move(error)) {}
    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const noexcept { return has_value(); }
    T& value() & { return std::get<T>(storage_); }
    const T& value() const & { return std::get<T>(storage_); }
    T&& value() && { return std::get<T>(std::move(storage_)); }
    const Error& error() const & { return std::get<Error>(storage_); }
private:
    std::variant<T, Error> storage_;
};

using Status = Result<std::monostate>;

} // namespace qualpf
