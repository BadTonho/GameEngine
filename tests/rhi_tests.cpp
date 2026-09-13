#include "engine/rhi/rhi.hpp"

#if GAMEENGINE_RENDERER_HAS_VULKAN
#include "engine/renderer/vulkan/vulkan_utils.hpp"

#include <cstdint>
#include <limits>
#include <vector>
#endif

int main()
{
    gameengine::rhi::Renderer renderer;
    gameengine::platform::Platform platform;

    if (renderer.is_initialized()) {
        return 1;
    }
#if GAMEENGINE_RENDERER_HAS_VULKAN
    constexpr auto expected_error = gameengine::core::ErrorCode::not_initialized;
#else
    constexpr auto expected_error = gameengine::core::ErrorCode::unsupported_platform;
#endif
    if (renderer.render_frame(platform).code != expected_error) {
        return 2;
    }

    renderer.shutdown();

#if GAMEENGINE_RENDERER_HAS_VULKAN
    const std::vector<VkSurfaceFormatKHR> formats = {
        VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    };
    if (gameengine::renderer::vulkan::choose_surface_format(formats).format !=
        VK_FORMAT_B8G8R8A8_SRGB) {
        return 3;
    }

    const std::vector<VkPresentModeKHR> present_modes = {
        VK_PRESENT_MODE_FIFO_KHR,
    };
    if (gameengine::renderer::vulkan::choose_present_mode(present_modes) !=
        VK_PRESENT_MODE_FIFO_KHR) {
        return 4;
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.currentExtent = {
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max(),
    };
    capabilities.minImageExtent = {320, 240};
    capabilities.maxImageExtent = {1920, 1080};
    const auto extent = gameengine::renderer::vulkan::choose_extent(
        capabilities,
        gameengine::platform::WindowSize{2560, 120});
    if (extent.width != 1920 || extent.height != 240) {
        return 5;
    }

    if (gameengine::renderer::vulkan::map_result(VK_SUCCESS,
                                                  gameengine::core::ErrorCode::vulkan_frame_failed)
            .code != gameengine::core::ErrorCode::none ||
        gameengine::renderer::vulkan::map_result(VK_ERROR_SURFACE_LOST_KHR,
                                                  gameengine::core::ErrorCode::vulkan_frame_failed)
                .code != gameengine::core::ErrorCode::vulkan_surface_lost) {
        return 6;
    }
#endif

    return 0;
}
