#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <X11/Xlib.h>
#undef Status
#else
#include <vulkan/vulkan.h>
#endif

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
