#pragma once

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#undef Status

#include <vector>

#include "engine/core/status.hpp"
#include "engine/platform/platform.hpp"

namespace gameengine::renderer::vulkan {

[[nodiscard]] VkSurfaceFormatKHR choose_surface_format(
    const std::vector<VkSurfaceFormatKHR>& formats) noexcept;

[[nodiscard]] VkPresentModeKHR choose_present_mode(
    const std::vector<VkPresentModeKHR>& present_modes) noexcept;

[[nodiscard]] VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                                        platform::WindowSize size) noexcept;

[[nodiscard]] core::Status map_result(VkResult result,
                                       core::ErrorCode fallback) noexcept;

} // namespace gameengine::renderer::vulkan
