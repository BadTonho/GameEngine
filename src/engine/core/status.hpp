#pragma once

#include "engine/core/types.hpp"

namespace gameengine::core {

enum class ErrorCode : u8 {
    none = 0,
    already_initialized,
    not_initialized,
    invalid_argument,
    allocation_failed,
    display_unavailable,
    window_creation_failed,
    unsupported_platform,
};

struct Status final {
    constexpr Status() noexcept = default;
    constexpr explicit Status(ErrorCode status_code) noexcept : code(status_code) {}

    [[nodiscard]] constexpr bool ok() const noexcept { return code == ErrorCode::none; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return ok(); }

    ErrorCode code = ErrorCode::none;
};

[[nodiscard]] constexpr const char* to_string(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::none:
        return "none";
    case ErrorCode::already_initialized:
        return "already_initialized";
    case ErrorCode::not_initialized:
        return "not_initialized";
    case ErrorCode::invalid_argument:
        return "invalid_argument";
    case ErrorCode::allocation_failed:
        return "allocation_failed";
    case ErrorCode::display_unavailable:
        return "display_unavailable";
    case ErrorCode::window_creation_failed:
        return "window_creation_failed";
    case ErrorCode::unsupported_platform:
        return "unsupported_platform";
    }

    return "unknown";
}

} // namespace gameengine::core
