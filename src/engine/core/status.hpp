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
    vulkan_instance_failed,
    vulkan_validation_unavailable,
    vulkan_surface_failed,
    vulkan_device_failed,
    vulkan_swapchain_failed,
    vulkan_frame_failed,
    vulkan_surface_lost,
    vulkan_validation_failed,
    shader_variant_unavailable,
    shader_reload_disabled,
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
    case ErrorCode::vulkan_instance_failed:
        return "vulkan_instance_failed";
    case ErrorCode::vulkan_validation_unavailable:
        return "vulkan_validation_unavailable";
    case ErrorCode::vulkan_surface_failed:
        return "vulkan_surface_failed";
    case ErrorCode::vulkan_device_failed:
        return "vulkan_device_failed";
    case ErrorCode::vulkan_swapchain_failed:
        return "vulkan_swapchain_failed";
    case ErrorCode::vulkan_frame_failed:
        return "vulkan_frame_failed";
    case ErrorCode::vulkan_surface_lost:
        return "vulkan_surface_lost";
    case ErrorCode::vulkan_validation_failed:
        return "vulkan_validation_failed";
    case ErrorCode::shader_variant_unavailable:
        return "shader_variant_unavailable";
    case ErrorCode::shader_reload_disabled:
        return "shader_reload_disabled";
    }

    return "unknown";
}

} // namespace gameengine::core
