#include "engine/renderer/vulkan/vulkan_utils.hpp"

#include <algorithm>
#include <limits>

namespace gameengine::renderer::vulkan {

VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& formats) noexcept
{
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    if (formats.empty()) {
        return {};
    }
    return formats.front();
}

VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& present_modes) noexcept
{
    for (const auto mode : present_modes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities,
                         platform::WindowSize size) noexcept
{
    if (capabilities.currentExtent.width != std::numeric_limits<core::u32>::max()) {
        return capabilities.currentExtent;
    }

    return {
        std::clamp(size.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(size.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}

core::Status map_result(VkResult result, core::ErrorCode fallback) noexcept
{
    if (result == VK_SUCCESS) {
        return core::Status{};
    }
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        return core::Status{core::ErrorCode::vulkan_surface_lost};
    }
    return core::Status{fallback};
}

} // namespace gameengine::renderer::vulkan
