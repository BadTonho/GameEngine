#include "engine/rhi/rhi.hpp"

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <X11/Xlib.h>

#undef Status

#include "engine/core/diagnostics.hpp"
#include "engine/renderer/vulkan/triangle_shaders.hpp"
#include "engine/renderer/vulkan/vulkan_utils.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <string_view>
#include <vector>

namespace gameengine::rhi {

namespace {

constexpr std::array<const char*, 1> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
constexpr core::u32 frames_in_flight = 2;

[[nodiscard]] bool has_extension(const std::vector<VkExtensionProperties>& extensions,
                                 const char* name) noexcept
{
    return std::any_of(extensions.begin(), extensions.end(), [name](const auto& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

[[nodiscard]] bool has_layer(const std::vector<VkLayerProperties>& layers,
                             const char* name) noexcept
{
    return std::any_of(layers.begin(), layers.end(), [name](const auto& layer) {
        return std::strcmp(layer.layerName, name) == 0;
    });
}

[[nodiscard]] bool is_debug_build() noexcept
{
#ifndef NDEBUG
    return true;
#else
    return false;
#endif
}

} // namespace

struct Renderer::Impl final {
    enum class ResourceState : core::u8 {
        free = 0,
        live,
        pending,
    };

    struct BufferSlot final {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
        ResourceState state = ResourceState::free;
        core::u32 generation = 1;
    };

    struct ImageSlot final {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        core::u32 width = 0;
        core::u32 height = 0;
        ResourceState state = ResourceState::free;
        core::u32 generation = 1;
    };

    struct SamplerSlot final {
        VkSampler sampler = VK_NULL_HANDLE;
        ResourceState state = ResourceState::free;
        core::u32 generation = 1;
    };

    struct PipelineSlot final {
        VkPipeline pipeline = VK_NULL_HANDLE;
        rhi::GraphicsPipelineDescription description{};
        ResourceState state = ResourceState::free;
        core::u32 generation = 1;
    };

    enum class DeferredResource : core::u8 {
        buffer = 0,
        image,
        sampler,
        pipeline,
    };

    struct DeferredDeletion final {
        DeferredResource resource = DeferredResource::buffer;
        core::u32 index = invalid_handle_index;
        core::u32 generation = 0;
    };

    struct QueueFamilies final {
        std::optional<core::u32> graphics;
        std::optional<core::u32> present;

        [[nodiscard]] bool complete() const noexcept
        {
            return graphics.has_value() && present.has_value();
        }
    };

    struct SwapchainSupport final {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent{};
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkCommandBuffer> command_buffers;

    std::array<VkSemaphore, frames_in_flight> image_available{};
    std::array<VkSemaphore, frames_in_flight> render_finished{};
    std::array<VkFence, frames_in_flight> in_flight_fences{};
    std::vector<VkFence> images_in_flight;
    core::u32 current_frame = 0;

    platform::NativeWindowHandles native_handles{};
    platform::WindowSize last_window_size{};
    std::vector<BufferSlot> buffers;
    std::vector<ImageSlot> images;
    std::vector<SamplerSlot> samplers;
    std::vector<PipelineSlot> pipelines;
    std::array<std::vector<DeferredDeletion>, frames_in_flight> deferred_deletions;
    rhi::BufferHandle triangle_buffer{};
    rhi::PipelineHandle triangle_pipeline{};
    PFN_vkSetDebugUtilsObjectNameEXT set_debug_utils_object_name = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_debug_utils_label = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_debug_utils_label = nullptr;
    bool validation_error = false;

    [[nodiscard]] core::Status initialize(platform::NativeWindowHandles handles,
                                           platform::WindowSize window_size) noexcept;
    void shutdown() noexcept;
    [[nodiscard]] core::Status render_frame(platform::WindowSize window_size) noexcept;

    [[nodiscard]] core::Status create_buffer(const rhi::BufferDescription& description,
                                              rhi::BufferHandle& handle) noexcept;
    [[nodiscard]] core::Status upload_buffer(rhi::BufferHandle handle,
                                              std::span<const std::byte> data) noexcept;
    [[nodiscard]] core::Status destroy_buffer(rhi::BufferHandle handle) noexcept;
    [[nodiscard]] core::Status create_image(const rhi::ImageDescription& description,
                                             rhi::ImageHandle& handle) noexcept;
    [[nodiscard]] core::Status upload_image(rhi::ImageHandle handle,
                                             std::span<const std::byte> data) noexcept;
    [[nodiscard]] core::Status destroy_image(rhi::ImageHandle handle) noexcept;
    [[nodiscard]] core::Status create_sampler(const rhi::SamplerDescription& description,
                                              rhi::SamplerHandle& handle) noexcept;
    [[nodiscard]] core::Status destroy_sampler(rhi::SamplerHandle handle) noexcept;
    [[nodiscard]] core::Status create_graphics_pipeline(
        const rhi::GraphicsPipelineDescription& description,
        rhi::PipelineHandle& handle) noexcept;
    [[nodiscard]] core::Status destroy_pipeline(rhi::PipelineHandle handle) noexcept;

    [[nodiscard]] core::Status create_instance() noexcept;
    [[nodiscard]] core::Status create_debug_messenger() noexcept;
    [[nodiscard]] core::Status create_surface() noexcept;
    [[nodiscard]] core::Status pick_physical_device() noexcept;
    [[nodiscard]] core::Status create_logical_device() noexcept;
    [[nodiscard]] core::Status create_command_pool() noexcept;
    [[nodiscard]] core::Status recreate_swapchain(platform::WindowSize window_size) noexcept;
    [[nodiscard]] core::Status create_swapchain(platform::WindowSize window_size) noexcept;
    [[nodiscard]] core::Status create_image_views() noexcept;
    [[nodiscard]] core::Status create_render_pass() noexcept;
    [[nodiscard]] core::Status create_framebuffers() noexcept;
    [[nodiscard]] core::Status create_command_buffers() noexcept;
    [[nodiscard]] core::Status create_sync_objects() noexcept;
    [[nodiscard]] core::Status create_triangle_resources() noexcept;
    [[nodiscard]] core::Status rebuild_pipelines() noexcept;
    [[nodiscard]] VkResult record_command_buffer(VkCommandBuffer command_buffer,
                                                  core::u32 image_index) noexcept;

    void cleanup_swapchain() noexcept;
    void collect_deferred(core::u32 frame_index) noexcept;
    void destroy_live_resources() noexcept;
    void release_deferred(const DeferredDeletion& deletion) noexcept;
    [[nodiscard]] core::Status create_buffer_resource(VkDeviceSize size,
                                                       VkBufferUsageFlags usage,
                                                       VkMemoryPropertyFlags properties,
                                                       VkBuffer& buffer,
                                                       VkDeviceMemory& memory) noexcept;
    void destroy_buffer_resource(VkBuffer buffer, VkDeviceMemory memory) noexcept;
    void destroy_image_resource(ImageSlot& slot) noexcept;
    [[nodiscard]] core::Status create_image_view(ImageSlot& slot) noexcept;
    [[nodiscard]] core::Status find_memory_type(std::uint32_t type_filter,
                                                 VkMemoryPropertyFlags properties,
                                                 std::uint32_t& memory_type) const noexcept;
    [[nodiscard]] VkCommandBuffer begin_one_time_commands() noexcept;
    [[nodiscard]] core::Status end_one_time_commands(VkCommandBuffer command_buffer) noexcept;
    [[nodiscard]] bool validate_buffer(rhi::BufferHandle handle,
                                       BufferSlot*& slot) noexcept;
    [[nodiscard]] bool validate_image(rhi::ImageHandle handle, ImageSlot*& slot) noexcept;
    [[nodiscard]] bool validate_sampler(rhi::SamplerHandle handle,
                                        SamplerSlot*& slot) noexcept;
    [[nodiscard]] bool validate_pipeline(rhi::PipelineHandle handle,
                                         PipelineSlot*& slot) noexcept;
    [[nodiscard]] core::Status allocate_buffer_slot(rhi::BufferHandle& handle) noexcept;
    [[nodiscard]] core::Status allocate_image_slot(rhi::ImageHandle& handle) noexcept;
    [[nodiscard]] core::Status allocate_sampler_slot(rhi::SamplerHandle& handle) noexcept;
    [[nodiscard]] core::Status allocate_pipeline_slot(rhi::PipelineHandle& handle) noexcept;
    [[nodiscard]] QueueFamilies find_queue_families(VkPhysicalDevice candidate) const noexcept;
    [[nodiscard]] bool check_device_extension_support(VkPhysicalDevice candidate) const noexcept;
    [[nodiscard]] bool query_swapchain_support(VkPhysicalDevice candidate,
                                                SwapchainSupport& support) const noexcept;
    [[nodiscard]] bool is_device_suitable(VkPhysicalDevice candidate,
                                          QueueFamilies& families) const noexcept;

    [[nodiscard]] VkShaderModule create_shader_module(const std::uint32_t* code,
                                                      std::size_t size) const noexcept;
    [[nodiscard]] core::Status create_pipeline_object(PipelineSlot& slot) noexcept;
    void destroy_pipeline_object(PipelineSlot& slot) noexcept;
    void set_debug_name(VkObjectType object_type,
                        std::uint64_t object,
                        const char* name) noexcept;
    void begin_debug_label(VkCommandBuffer command_buffer, const char* name) noexcept;
    void end_debug_label(VkCommandBuffer command_buffer) noexcept;
};

namespace {

constexpr const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";

[[nodiscard]] core::u32 next_generation(core::u32 generation) noexcept
{
    return generation == std::numeric_limits<core::u32>::max() ? 1U : generation + 1U;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                               VkDebugUtilsMessageTypeFlagsEXT,
                                               const VkDebugUtilsMessengerCallbackDataEXT* data,
                                               void* user_data) noexcept
{
    auto* renderer = static_cast<Renderer::Impl*>(user_data);
    const auto level = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0
                           ? core::LogLevel::error
                           : core::LogLevel::warning;
    if (level == core::LogLevel::error) {
        renderer->validation_error = true;
    }
    core::log(level, data->pMessage == nullptr ? std::string_view{"Vulkan validation message"}
                                               : std::string_view{data->pMessage});
    return VK_FALSE;
}

[[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT debug_create_info(
    Renderer::Impl* renderer) noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debug_callback;
    info.pUserData = renderer;
    return info;
}

[[nodiscard]] VkResult create_debug_utils_messenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* create_info,
    VkDebugUtilsMessengerEXT* messenger) noexcept
{
    const auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (function == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return function(instance, create_info, nullptr, messenger);
}

void destroy_debug_utils_messenger(VkInstance instance,
                                   VkDebugUtilsMessengerEXT messenger) noexcept
{
    const auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (function != nullptr) {
        function(instance, messenger, nullptr);
    }
}

} // namespace

core::Status Renderer::Impl::initialize(platform::NativeWindowHandles handles,
                                         platform::WindowSize window_size) noexcept
{
    if (!handles.valid() || window_size.width == 0 || window_size.height == 0) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    native_handles = handles;
    last_window_size = window_size;

    core::Status status = create_instance();
    if (!status) {
        return status;
    }
    status = create_surface();
    if (!status) {
        return status;
    }
    status = pick_physical_device();
    if (!status) {
        return status;
    }
    status = create_logical_device();
    if (!status) {
        return status;
    }
    status = create_command_pool();
    if (!status) {
        return status;
    }
    status = create_swapchain(window_size);
    if (!status) {
        return status;
    }
    status = create_sync_objects();
    if (!status) {
        return status;
    }
    status = create_triangle_resources();
    if (!status) {
        return status;
    }

    return validation_error ? core::Status{core::ErrorCode::vulkan_validation_failed}
                            : core::Status{};
}

void Renderer::Impl::shutdown() noexcept
{
    if (device != VK_NULL_HANDLE) {
        static_cast<void>(vkDeviceWaitIdle(device));
    }

    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        collect_deferred(index);
    }

    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        if (image_available[index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, image_available[index], nullptr);
        }
        if (render_finished[index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, render_finished[index], nullptr);
        }
        if (in_flight_fences[index] != VK_NULL_HANDLE) {
            vkDestroyFence(device, in_flight_fences[index], nullptr);
        }
    }

    cleanup_swapchain();
    destroy_live_resources();

    if (device != VK_NULL_HANDLE && pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }

    if (device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, command_pool, nullptr);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    if (debug_messenger != VK_NULL_HANDLE) {
        destroy_debug_utils_messenger(instance, debug_messenger);
        debug_messenger = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    physical_device = VK_NULL_HANDLE;
    graphics_queue = VK_NULL_HANDLE;
    present_queue = VK_NULL_HANDLE;
    command_pool = VK_NULL_HANDLE;
    native_handles = {};
    last_window_size = {};
    triangle_buffer = {};
    triangle_pipeline = {};
    validation_error = false;
}

core::Status Renderer::Impl::create_instance() noexcept
{
    std::uint32_t layer_count = 0;
    if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_instance_failed};
    }
    std::vector<VkLayerProperties> available_layers(layer_count);
    if (layer_count > 0 &&
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data()) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_instance_failed};
    }

    const bool validation = is_debug_build();
    if (validation && !has_layer(available_layers, validation_layer_name)) {
        return core::Status{core::ErrorCode::vulkan_validation_unavailable};
    }

    std::uint32_t extension_count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_instance_failed};
    }
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    if (extension_count > 0 &&
        vkEnumerateInstanceExtensionProperties(nullptr,
                                               &extension_count,
                                               available_extensions.data()) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_instance_failed};
    }

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    };
    if (validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    for (const char* extension : extensions) {
        if (!has_extension(available_extensions, extension)) {
            return core::Status{core::ErrorCode::vulkan_instance_failed};
        }
    }

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "GameEngine";
    application_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    application_info.pEngineName = "GameEngine";
    application_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    application_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    create_info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    if (validation) {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = &validation_layer_name;
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_info{};
    if (validation) {
        debug_info = debug_create_info(this);
        create_info.pNext = &debug_info;
    }

    if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_instance_failed};
    }

    if (validation) {
        return create_debug_messenger();
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_debug_messenger() noexcept
{
    set_debug_utils_object_name = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
    cmd_begin_debug_utils_label = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
    cmd_end_debug_utils_label = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));

    const VkDebugUtilsMessengerCreateInfoEXT info = debug_create_info(this);
    if (create_debug_utils_messenger(instance, &info, &debug_messenger) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_instance_failed};
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_surface() noexcept
{
    auto* display = reinterpret_cast<Display*>(native_handles.display);
    const auto window = static_cast<::Window>(native_handles.window);
    VkXlibSurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.dpy = display;
    create_info.window = window;
    if (vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, &surface) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_surface_failed};
    }
    return core::Status{};
}

Renderer::Impl::QueueFamilies Renderer::Impl::find_queue_families(
    VkPhysicalDevice candidate) const noexcept
{
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &count, families.data());

    QueueFamilies result;
    for (core::u32 index = 0; index < count; ++index) {
        if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            result.graphics = index;
        }
        VkBool32 present_support = VK_FALSE;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &present_support) ==
                VK_SUCCESS &&
            present_support == VK_TRUE) {
            result.present = index;
        }
        if (result.complete()) {
            break;
        }
    }
    return result;
}

bool Renderer::Impl::check_device_extension_support(VkPhysicalDevice candidate) const noexcept
{
    std::uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> available(count);
    if (count > 0 &&
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &count, available.data()) !=
            VK_SUCCESS) {
        return false;
    }
    return std::all_of(device_extensions.begin(), device_extensions.end(),
                       [&available](const char* name) { return has_extension(available, name); });
}

bool Renderer::Impl::query_swapchain_support(VkPhysicalDevice candidate,
                                               SwapchainSupport& support) const noexcept
{
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, surface, &support.capabilities) !=
        VK_SUCCESS) {
        return false;
    }

    std::uint32_t format_count = 0;
    std::uint32_t present_mode_count = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &format_count, nullptr) !=
            VK_SUCCESS ||
        vkGetPhysicalDeviceSurfacePresentModesKHR(candidate,
                                                   surface,
                                                   &present_mode_count,
                                                   nullptr) != VK_SUCCESS) {
        return false;
    }
    support.formats.resize(format_count);
    support.present_modes.resize(present_mode_count);
    if (format_count > 0 &&
        vkGetPhysicalDeviceSurfaceFormatsKHR(candidate,
                                             surface,
                                             &format_count,
                                             support.formats.data()) != VK_SUCCESS) {
        return false;
    }
    if (present_mode_count > 0 &&
        vkGetPhysicalDeviceSurfacePresentModesKHR(candidate,
                                                   surface,
                                                   &present_mode_count,
                                                   support.present_modes.data()) != VK_SUCCESS) {
        return false;
    }
    return !support.formats.empty() && !support.present_modes.empty();
}

bool Renderer::Impl::is_device_suitable(VkPhysicalDevice candidate,
                                         QueueFamilies& families) const noexcept
{
    families = find_queue_families(candidate);
    if (!families.complete() || !check_device_extension_support(candidate)) {
        return false;
    }

    SwapchainSupport support;
    return query_swapchain_support(candidate, support);
}

core::Status Renderer::Impl::pick_physical_device() noexcept
{
    std::uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    std::vector<VkPhysicalDevice> devices(count);
    if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    int best_score = -1;
    QueueFamilies best_families;
    for (VkPhysicalDevice candidate : devices) {
        QueueFamilies families;
        if (!is_device_suitable(candidate, families)) {
            continue;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        int score = static_cast<int>(properties.limits.maxImageDimension2D);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 100000;
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 50000;
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            score += 1000;
        }

        if (score > best_score) {
            best_score = score;
            physical_device = candidate;
            best_families = families;
        }
    }

    if (physical_device == VK_NULL_HANDLE) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_logical_device() noexcept
{
    const QueueFamilies families = find_queue_families(physical_device);
    std::vector<core::u32> unique_families = {families.graphics.value()};
    if (families.present.value() != families.graphics.value()) {
        unique_families.push_back(families.present.value());
    }

    constexpr float queue_priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    queue_infos.reserve(unique_families.size());
    for (const core::u32 family : unique_families) {
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_infos.push_back(queue_info);
    }

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_infos.size());
    create_info.pQueueCreateInfos = queue_infos.data();
    create_info.pEnabledFeatures = &features;
    create_info.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    if (vkCreateDevice(physical_device, &create_info, nullptr, &device) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    vkGetDeviceQueue(device, families.graphics.value(), 0, &graphics_queue);
    vkGetDeviceQueue(device, families.present.value(), 0, &present_queue);
    return core::Status{};
}

core::Status Renderer::Impl::create_command_pool() noexcept
{
    const QueueFamilies families = find_queue_families(physical_device);
    VkCommandPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = families.graphics.value();
    if (vkCreateCommandPool(device, &create_info, nullptr, &command_pool) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_swapchain(platform::WindowSize window_size) noexcept
{
    SwapchainSupport support;
    if (!query_swapchain_support(physical_device, support)) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    const VkSurfaceFormatKHR surface_format =
        ::gameengine::renderer::vulkan::choose_surface_format(support.formats);
    const VkPresentModeKHR present_mode =
        ::gameengine::renderer::vulkan::choose_present_mode(support.present_modes);
    swapchain_extent =
        ::gameengine::renderer::vulkan::choose_extent(support.capabilities, window_size);

    std::uint32_t image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    const QueueFamilies families = find_queue_families(physical_device);
    const std::array<core::u32, 2> queue_family_indices = {
        families.graphics.value(),
        families.present.value(),
    };
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (families.graphics.value() != families.present.value()) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    std::uint32_t actual_image_count = 0;
    if (vkGetSwapchainImagesKHR(device, swapchain, &actual_image_count, nullptr) != VK_SUCCESS ||
        actual_image_count == 0) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }
    swapchain_images.resize(actual_image_count);
    if (vkGetSwapchainImagesKHR(device,
                                swapchain,
                                &actual_image_count,
                                swapchain_images.data()) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }
    swapchain_format = surface_format.format;

    core::Status status = create_image_views();
    if (!status) {
        return status;
    }
    status = create_render_pass();
    if (!status) {
        return status;
    }
    if (triangle_pipeline.valid()) {
        status = rebuild_pipelines();
    } else {
        rhi::GraphicsPipelineDescription description{};
        status = create_graphics_pipeline(description, triangle_pipeline);
    }
    if (!status) {
        return status;
    }
    status = create_framebuffers();
    if (!status) {
        return status;
    }
    status = create_command_buffers();
    if (!status) {
        return status;
    }

    images_in_flight.assign(swapchain_images.size(), VK_NULL_HANDLE);
    last_window_size = window_size;
    return validation_error ? core::Status{core::ErrorCode::vulkan_validation_failed}
                            : core::Status{};
}

core::Status Renderer::Impl::create_image_views() noexcept
{
    swapchain_image_views.resize(swapchain_images.size());
    for (std::size_t index = 0; index < swapchain_images.size(); ++index) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swapchain_images[index];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = swapchain_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device,
                              &create_info,
                              nullptr,
                              &swapchain_image_views[index]) != VK_SUCCESS) {
            return core::Status{core::ErrorCode::vulkan_swapchain_failed};
        }
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_render_pass() noexcept
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_reference{};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;
    if (vkCreateRenderPass(device, &create_info, nullptr, &render_pass) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }
    return core::Status{};
}

VkShaderModule Renderer::Impl::create_shader_module(const std::uint32_t* code,
                                                    std::size_t size) const noexcept
{
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = size;
    create_info.pCode = code;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return shader_module;
}

core::Status Renderer::Impl::create_pipeline_object(PipelineSlot& slot) noexcept
{
    if (slot.description.vertex_layout != rhi::PipelineVertexLayout::position2_color3) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    const VkShaderModule vertex_shader = create_shader_module(
        ::gameengine::renderer::vulkan::bootstrap::vertex_shader.data(),
        ::gameengine::renderer::vulkan::bootstrap::vertex_shader.size() * sizeof(std::uint32_t));
    const VkShaderModule fragment_shader = create_shader_module(
        ::gameengine::renderer::vulkan::bootstrap::fragment_shader.data(),
        ::gameengine::renderer::vulkan::bootstrap::fragment_shader.size() * sizeof(std::uint32_t));
    if (vertex_shader == VK_NULL_HANDLE || fragment_shader == VK_NULL_HANDLE) {
        if (vertex_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertex_shader, nullptr);
        }
        if (fragment_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragment_shader, nullptr);
        }
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    VkVertexInputBindingDescription vertex_binding{};
    vertex_binding.binding = 0;
    vertex_binding.stride = sizeof(float) * 5;
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    const std::array<VkVertexInputAttributeDescription, 2> vertex_attributes = {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 2},
    };
    VkPipelineShaderStageCreateInfo vertex_stage{};
    vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage.module = vertex_shader;
    vertex_stage.pName = "main";
    VkPipelineShaderStageCreateInfo fragment_stage{};
    fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_stage.module = fragment_shader;
    fragment_stage.pName = "main";
    const std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertex_stage, fragment_stage};

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_binding;
    vertex_input.vertexAttributeDescriptionCount =
        static_cast<std::uint32_t>(vertex_attributes.size());
    vertex_input.pVertexAttributeDescriptions = vertex_attributes.data();
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_extent.width);
    viewport.height = static_cast<float>(swapchain_extent.height);
    viewport.maxDepth = 1.0F;
    VkRect2D scissor{};
    scissor.extent = swapchain_extent;
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0F;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                             VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT |
                                             VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    const std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    bool created_layout = false;
    if (pipeline_layout == VK_NULL_HANDLE) {
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        if (vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
            vkDestroyShaderModule(device, vertex_shader, nullptr);
            vkDestroyShaderModule(device, fragment_shader, nullptr);
            return core::Status{core::ErrorCode::vulkan_swapchain_failed};
        }
        created_layout = true;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<std::uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    if (vkCreateGraphicsPipelines(device,
                                  VK_NULL_HANDLE,
                                  1,
                                  &pipeline_info,
                                  nullptr,
                                  &slot.pipeline) != VK_SUCCESS) {
        if (created_layout) {
            vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
            pipeline_layout = VK_NULL_HANDLE;
        }
        vkDestroyShaderModule(device, vertex_shader, nullptr);
        vkDestroyShaderModule(device, fragment_shader, nullptr);
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    vkDestroyShaderModule(device, vertex_shader, nullptr);
    vkDestroyShaderModule(device, fragment_shader, nullptr);
    set_debug_name(VK_OBJECT_TYPE_PIPELINE,
                   reinterpret_cast<std::uint64_t>(slot.pipeline),
                   "GameEngine.TrianglePipeline");
    return core::Status{};
}

core::Status Renderer::Impl::create_framebuffers() noexcept
{
    framebuffers.resize(swapchain_image_views.size());
    for (std::size_t index = 0; index < swapchain_image_views.size(); ++index) {
        VkImageView attachments[] = {swapchain_image_views[index]};
        VkFramebufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = attachments;
        create_info.width = swapchain_extent.width;
        create_info.height = swapchain_extent.height;
        create_info.layers = 1;
        if (vkCreateFramebuffer(device, &create_info, nullptr, &framebuffers[index]) != VK_SUCCESS) {
            return core::Status{core::ErrorCode::vulkan_swapchain_failed};
        }
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_command_buffers() noexcept
{
    command_buffers.resize(framebuffers.size());
    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = static_cast<std::uint32_t>(command_buffers.size());
    if (vkAllocateCommandBuffers(device, &allocate_info, command_buffers.data()) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_sync_objects() noexcept
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        if (vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available[index]) !=
                VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished[index]) !=
                VK_SUCCESS ||
            vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[index]) != VK_SUCCESS) {
            return core::Status{core::ErrorCode::vulkan_device_failed};
        }
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_triangle_resources() noexcept
{
    struct Vertex final {
        float position[2];
        float color[3];
    };

    constexpr std::array<Vertex, 3> vertices = {
        Vertex{{0.0F, -0.6F}, {1.0F, 0.2F, 0.2F}},
        Vertex{{0.6F, 0.6F}, {0.2F, 1.0F, 0.2F}},
        Vertex{{-0.6F, 0.6F}, {0.2F, 0.4F, 1.0F}},
    };

    const rhi::BufferDescription description{
        .size = sizeof(vertices),
        .usage = rhi::BufferUsage::vertex,
    };
    core::Status status = create_buffer(description, triangle_buffer);
    if (!status) {
        return status;
    }

    const auto vertex_bytes = std::as_bytes(std::span<const Vertex>{vertices});
    status = upload_buffer(triangle_buffer, vertex_bytes);
    if (!status) {
        return status;
    }
    return core::Status{};
}

core::Status Renderer::Impl::find_memory_type(std::uint32_t type_filter,
                                              VkMemoryPropertyFlags properties,
                                              std::uint32_t& memory_type) const noexcept
{
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
        if ((type_filter & (1U << index)) != 0 &&
            (memory_properties.memoryTypes[index].propertyFlags & properties) == properties) {
            memory_type = index;
            return core::Status{};
        }
    }
    return core::Status{core::ErrorCode::vulkan_device_failed};
}

core::Status Renderer::Impl::create_buffer_resource(VkDeviceSize size,
                                                    VkBufferUsageFlags usage,
                                                    VkMemoryPropertyFlags properties,
                                                    VkBuffer& buffer,
                                                    VkDeviceMemory& memory) noexcept
{
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    std::uint32_t memory_type = 0;
    core::Status status = find_memory_type(requirements.memoryTypeBits, properties, memory_type);
    if (!status) {
        destroy_buffer_resource(buffer, memory);
        return status;
    }

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = requirements.size;
    allocation_info.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(device, &allocation_info, nullptr, &memory) != VK_SUCCESS) {
        destroy_buffer_resource(buffer, memory);
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) {
        destroy_buffer_resource(buffer, memory);
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    return core::Status{};
}

void Renderer::Impl::destroy_buffer_resource(VkBuffer buffer, VkDeviceMemory memory) noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
    }
}

VkCommandBuffer Renderer::Impl::begin_one_time_commands() noexcept
{
    VkCommandBufferAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.commandPool = command_pool;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandBufferCount = 1;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocation_info, &command_buffer) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
        return VK_NULL_HANDLE;
    }
    return command_buffer;
}

core::Status Renderer::Impl::end_one_time_commands(VkCommandBuffer command_buffer) noexcept
{
    if (command_buffer == VK_NULL_HANDLE || vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        if (command_buffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
        }
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    const VkResult submit_result = vkQueueSubmit(graphics_queue, 1, &submit_info, fence);
    const VkResult wait_result = submit_result == VK_SUCCESS
                                     ? vkWaitForFences(device, 1, &fence, VK_TRUE,
                                                      std::numeric_limits<std::uint64_t>::max())
                                     : submit_result;
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    return wait_result == VK_SUCCESS ? core::Status{}
                                     : core::Status{core::ErrorCode::vulkan_frame_failed};
}

core::Status Renderer::Impl::allocate_buffer_slot(rhi::BufferHandle& handle) noexcept
{
    for (core::u32 index = 0; index < buffers.size(); ++index) {
        if (buffers[index].state == ResourceState::free) {
            buffers[index].state = ResourceState::live;
            handle = {index, buffers[index].generation};
            return core::Status{};
        }
    }
    buffers.emplace_back();
    const core::u32 index = static_cast<core::u32>(buffers.size() - 1);
    buffers[index].state = ResourceState::live;
    handle = {index, buffers[index].generation};
    return core::Status{};
}

core::Status Renderer::Impl::allocate_image_slot(rhi::ImageHandle& handle) noexcept
{
    for (core::u32 index = 0; index < images.size(); ++index) {
        if (images[index].state == ResourceState::free) {
            images[index].state = ResourceState::live;
            handle = {index, images[index].generation};
            return core::Status{};
        }
    }
    images.emplace_back();
    const core::u32 index = static_cast<core::u32>(images.size() - 1);
    images[index].state = ResourceState::live;
    handle = {index, images[index].generation};
    return core::Status{};
}

core::Status Renderer::Impl::allocate_sampler_slot(rhi::SamplerHandle& handle) noexcept
{
    for (core::u32 index = 0; index < samplers.size(); ++index) {
        if (samplers[index].state == ResourceState::free) {
            samplers[index].state = ResourceState::live;
            handle = {index, samplers[index].generation};
            return core::Status{};
        }
    }
    samplers.emplace_back();
    const core::u32 index = static_cast<core::u32>(samplers.size() - 1);
    samplers[index].state = ResourceState::live;
    handle = {index, samplers[index].generation};
    return core::Status{};
}

core::Status Renderer::Impl::allocate_pipeline_slot(rhi::PipelineHandle& handle) noexcept
{
    for (core::u32 index = 0; index < pipelines.size(); ++index) {
        if (pipelines[index].state == ResourceState::free) {
            pipelines[index].state = ResourceState::live;
            handle = {index, pipelines[index].generation};
            return core::Status{};
        }
    }
    pipelines.emplace_back();
    const core::u32 index = static_cast<core::u32>(pipelines.size() - 1);
    pipelines[index].state = ResourceState::live;
    handle = {index, pipelines[index].generation};
    return core::Status{};
}

bool Renderer::Impl::validate_buffer(rhi::BufferHandle handle, BufferSlot*& slot) noexcept
{
    if (!handle.valid() || handle.index >= buffers.size()) {
        return false;
    }
    slot = &buffers[handle.index];
    return slot->state == ResourceState::live && slot->generation == handle.generation;
}

bool Renderer::Impl::validate_image(rhi::ImageHandle handle, ImageSlot*& slot) noexcept
{
    if (!handle.valid() || handle.index >= images.size()) {
        return false;
    }
    slot = &images[handle.index];
    return slot->state == ResourceState::live && slot->generation == handle.generation;
}

bool Renderer::Impl::validate_sampler(rhi::SamplerHandle handle, SamplerSlot*& slot) noexcept
{
    if (!handle.valid() || handle.index >= samplers.size()) {
        return false;
    }
    slot = &samplers[handle.index];
    return slot->state == ResourceState::live && slot->generation == handle.generation;
}

bool Renderer::Impl::validate_pipeline(rhi::PipelineHandle handle, PipelineSlot*& slot) noexcept
{
    if (!handle.valid() || handle.index >= pipelines.size()) {
        return false;
    }
    slot = &pipelines[handle.index];
    return slot->state == ResourceState::live && slot->generation == handle.generation;
}

core::Status Renderer::Impl::create_buffer(const rhi::BufferDescription& description,
                                           rhi::BufferHandle& handle) noexcept
{
    handle = {};
    if (description.size == 0 || description.size > std::numeric_limits<VkDeviceSize>::max() ||
        description.usage != rhi::BufferUsage::vertex) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    core::Status status = allocate_buffer_slot(handle);
    if (!status) {
        return status;
    }
    BufferSlot& slot = buffers[handle.index];
    status = create_buffer_resource(static_cast<VkDeviceSize>(description.size),
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    slot.buffer,
                                    slot.memory);
    if (!status) {
        slot.state = ResourceState::free;
        handle = {};
        return status;
    }
    slot.size = static_cast<VkDeviceSize>(description.size);
    set_debug_name(VK_OBJECT_TYPE_BUFFER,
                   reinterpret_cast<std::uint64_t>(slot.buffer),
                   "GameEngine.VertexBuffer");
    return core::Status{};
}

core::Status Renderer::Impl::upload_buffer(rhi::BufferHandle handle,
                                           std::span<const std::byte> data) noexcept
{
    BufferSlot* destination = nullptr;
    if (!validate_buffer(handle, destination) || data.empty() ||
        data.size() > static_cast<std::size_t>(destination->size)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    core::Status status = create_buffer_resource(
        static_cast<VkDeviceSize>(data.size()),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer,
        staging_memory);
    if (!status) {
        return status;
    }

    void* mapped = nullptr;
    if (vkMapMemory(device, staging_memory, 0, data.size(), 0, &mapped) != VK_SUCCESS) {
        destroy_buffer_resource(staging_buffer, staging_memory);
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    std::memcpy(mapped, data.data(), data.size());
    vkUnmapMemory(device, staging_memory);

    const VkCommandBuffer command_buffer = begin_one_time_commands();
    if (command_buffer == VK_NULL_HANDLE) {
        destroy_buffer_resource(staging_buffer, staging_memory);
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }
    begin_debug_label(command_buffer, "GameEngine.UploadBuffer");
    VkBufferCopy copy{};
    copy.size = data.size();
    vkCmdCopyBuffer(command_buffer, staging_buffer, destination->buffer, 1, &copy);
    end_debug_label(command_buffer);
    status = end_one_time_commands(command_buffer);
    destroy_buffer_resource(staging_buffer, staging_memory);
    return status;
}

core::Status Renderer::Impl::destroy_buffer(rhi::BufferHandle handle) noexcept
{
    BufferSlot* slot = nullptr;
    if (!validate_buffer(handle, slot)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    slot->state = ResourceState::pending;
    deferred_deletions[current_frame].push_back(
        {DeferredResource::buffer, handle.index, handle.generation});
    return core::Status{};
}

core::Status Renderer::Impl::create_image_view(ImageSlot& slot) noexcept
{
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = slot.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &view_info, nullptr, &slot.view) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    set_debug_name(VK_OBJECT_TYPE_IMAGE_VIEW,
                   reinterpret_cast<std::uint64_t>(slot.view),
                   "GameEngine.UploadImageView");
    return core::Status{};
}

void Renderer::Impl::destroy_image_resource(ImageSlot& slot) noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    if (slot.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, slot.view, nullptr);
        slot.view = VK_NULL_HANDLE;
    }
    if (slot.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, slot.image, nullptr);
        slot.image = VK_NULL_HANDLE;
    }
    if (slot.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, slot.memory, nullptr);
        slot.memory = VK_NULL_HANDLE;
    }
}

core::Status Renderer::Impl::create_image(const rhi::ImageDescription& description,
                                          rhi::ImageHandle& handle) noexcept
{
    handle = {};
    if (description.width == 0 || description.height == 0 ||
        description.format != rhi::ImageFormat::rgba8_unorm) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    core::Status status = allocate_image_slot(handle);
    if (!status) {
        return status;
    }
    ImageSlot& slot = images[handle.index];
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent = {description.width, description.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &image_info, nullptr, &slot.image) != VK_SUCCESS) {
        slot.state = ResourceState::free;
        handle = {};
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, slot.image, &requirements);
    std::uint32_t memory_type = 0;
    status = find_memory_type(requirements.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               memory_type);
    if (!status) {
        destroy_image_resource(slot);
        slot.state = ResourceState::free;
        handle = {};
        return status;
    }

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = requirements.size;
    allocation_info.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(device, &allocation_info, nullptr, &slot.memory) != VK_SUCCESS ||
        vkBindImageMemory(device, slot.image, slot.memory, 0) != VK_SUCCESS) {
        destroy_image_resource(slot);
        slot.state = ResourceState::free;
        handle = {};
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    status = create_image_view(slot);
    if (!status) {
        destroy_image_resource(slot);
        slot.state = ResourceState::free;
        handle = {};
        return status;
    }
    slot.width = description.width;
    slot.height = description.height;
    set_debug_name(VK_OBJECT_TYPE_IMAGE,
                   reinterpret_cast<std::uint64_t>(slot.image),
                   "GameEngine.UploadImage");
    return core::Status{};
}

core::Status Renderer::Impl::upload_image(rhi::ImageHandle handle,
                                          std::span<const std::byte> data) noexcept
{
    ImageSlot* destination = nullptr;
    if (!validate_image(handle, destination)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const std::uint64_t expected_size = static_cast<std::uint64_t>(destination->width) *
                                        static_cast<std::uint64_t>(destination->height) * 4U;
    if (data.empty() || data.size() != expected_size) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    core::Status status = create_buffer_resource(
        static_cast<VkDeviceSize>(data.size()),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging_buffer,
        staging_memory);
    if (!status) {
        return status;
    }
    void* mapped = nullptr;
    if (vkMapMemory(device, staging_memory, 0, data.size(), 0, &mapped) != VK_SUCCESS) {
        destroy_buffer_resource(staging_buffer, staging_memory);
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    std::memcpy(mapped, data.data(), data.size());
    vkUnmapMemory(device, staging_memory);

    const VkCommandBuffer command_buffer = begin_one_time_commands();
    if (command_buffer == VK_NULL_HANDLE) {
        destroy_buffer_resource(staging_buffer, staging_memory);
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }
    begin_debug_label(command_buffer, "GameEngine.UploadImage");

    VkImageMemoryBarrier to_transfer{};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = destination->image;
    to_transfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_transfer.subresourceRange.levelCount = 1;
    to_transfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &to_transfer);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {destination->width, destination->height, 1};
    vkCmdCopyBufferToImage(command_buffer,
                           staging_buffer,
                           destination->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &copy);

    VkImageMemoryBarrier to_shader{};
    to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.image = destination->image;
    to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_shader.subresourceRange.levelCount = 1;
    to_shader.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &to_shader);
    end_debug_label(command_buffer);
    status = end_one_time_commands(command_buffer);
    destroy_buffer_resource(staging_buffer, staging_memory);
    return status;
}

core::Status Renderer::Impl::destroy_image(rhi::ImageHandle handle) noexcept
{
    ImageSlot* slot = nullptr;
    if (!validate_image(handle, slot)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    slot->state = ResourceState::pending;
    deferred_deletions[current_frame].push_back(
        {DeferredResource::image, handle.index, handle.generation});
    return core::Status{};
}

core::Status Renderer::Impl::create_sampler(const rhi::SamplerDescription& description,
                                            rhi::SamplerHandle& handle) noexcept
{
    handle = {};
    if ((description.min_filter != rhi::SamplerFilter::nearest &&
         description.min_filter != rhi::SamplerFilter::linear) ||
        (description.mag_filter != rhi::SamplerFilter::nearest &&
         description.mag_filter != rhi::SamplerFilter::linear)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    core::Status status = allocate_sampler_slot(handle);
    if (!status) {
        return status;
    }
    auto filter = [](rhi::SamplerFilter value) noexcept {
        return value == rhi::SamplerFilter::nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    };
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = filter(description.mag_filter);
    sampler_info.minFilter = filter(description.min_filter);
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxLod = 1.0F;
    SamplerSlot& slot = samplers[handle.index];
    if (vkCreateSampler(device, &sampler_info, nullptr, &slot.sampler) != VK_SUCCESS) {
        slot.state = ResourceState::free;
        handle = {};
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    set_debug_name(VK_OBJECT_TYPE_SAMPLER,
                   reinterpret_cast<std::uint64_t>(slot.sampler),
                   "GameEngine.UploadSampler");
    return core::Status{};
}

core::Status Renderer::Impl::destroy_sampler(rhi::SamplerHandle handle) noexcept
{
    SamplerSlot* slot = nullptr;
    if (!validate_sampler(handle, slot)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    slot->state = ResourceState::pending;
    deferred_deletions[current_frame].push_back(
        {DeferredResource::sampler, handle.index, handle.generation});
    return core::Status{};
}

core::Status Renderer::Impl::create_graphics_pipeline(
    const rhi::GraphicsPipelineDescription& description,
    rhi::PipelineHandle& handle) noexcept
{
    handle = {};
    if (description.vertex_layout != rhi::PipelineVertexLayout::position2_color3 ||
        render_pass == VK_NULL_HANDLE) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    core::Status status = allocate_pipeline_slot(handle);
    if (!status) {
        return status;
    }
    PipelineSlot& slot = pipelines[handle.index];
    slot.description = description;
    status = create_pipeline_object(slot);
    if (!status) {
        slot.state = ResourceState::free;
        handle = {};
        return status;
    }
    return core::Status{};
}

core::Status Renderer::Impl::rebuild_pipelines() noexcept
{
    for (PipelineSlot& slot : pipelines) {
        if (slot.state == ResourceState::live) {
            const core::Status status = create_pipeline_object(slot);
            if (!status) {
                return status;
            }
        }
    }
    return core::Status{};
}

core::Status Renderer::Impl::destroy_pipeline(rhi::PipelineHandle handle) noexcept
{
    PipelineSlot* slot = nullptr;
    if (!validate_pipeline(handle, slot)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (handle.index == triangle_pipeline.index && handle.generation == triangle_pipeline.generation) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    slot->state = ResourceState::pending;
    deferred_deletions[current_frame].push_back(
        {DeferredResource::pipeline, handle.index, handle.generation});
    return core::Status{};
}

void Renderer::Impl::destroy_pipeline_object(PipelineSlot& slot) noexcept
{
    if (device != VK_NULL_HANDLE && slot.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, slot.pipeline, nullptr);
        slot.pipeline = VK_NULL_HANDLE;
    }
}

void Renderer::Impl::release_deferred(const DeferredDeletion& deletion) noexcept
{
    switch (deletion.resource) {
    case DeferredResource::buffer:
        if (deletion.index < buffers.size()) {
            BufferSlot& slot = buffers[deletion.index];
            if (slot.state == ResourceState::pending && slot.generation == deletion.generation) {
                destroy_buffer_resource(slot.buffer, slot.memory);
                slot.buffer = VK_NULL_HANDLE;
                slot.memory = VK_NULL_HANDLE;
                slot.size = 0;
                slot.state = ResourceState::free;
                slot.generation = next_generation(slot.generation);
            }
        }
        break;
    case DeferredResource::image:
        if (deletion.index < images.size()) {
            ImageSlot& slot = images[deletion.index];
            if (slot.state == ResourceState::pending && slot.generation == deletion.generation) {
                destroy_image_resource(slot);
                slot.width = 0;
                slot.height = 0;
                slot.state = ResourceState::free;
                slot.generation = next_generation(slot.generation);
            }
        }
        break;
    case DeferredResource::sampler:
        if (deletion.index < samplers.size()) {
            SamplerSlot& slot = samplers[deletion.index];
            if (slot.state == ResourceState::pending && slot.generation == deletion.generation) {
                if (slot.sampler != VK_NULL_HANDLE) {
                    vkDestroySampler(device, slot.sampler, nullptr);
                    slot.sampler = VK_NULL_HANDLE;
                }
                slot.state = ResourceState::free;
                slot.generation = next_generation(slot.generation);
            }
        }
        break;
    case DeferredResource::pipeline:
        if (deletion.index < pipelines.size()) {
            PipelineSlot& slot = pipelines[deletion.index];
            if (slot.state == ResourceState::pending && slot.generation == deletion.generation) {
                destroy_pipeline_object(slot);
                slot.state = ResourceState::free;
                slot.generation = next_generation(slot.generation);
            }
        }
        break;
    }
}

void Renderer::Impl::collect_deferred(core::u32 frame_index) noexcept
{
    if (frame_index >= deferred_deletions.size()) {
        return;
    }
    for (const DeferredDeletion& deletion : deferred_deletions[frame_index]) {
        release_deferred(deletion);
    }
    deferred_deletions[frame_index].clear();
}

void Renderer::Impl::destroy_live_resources() noexcept
{
    for (BufferSlot& slot : buffers) {
        if (slot.state != ResourceState::free) {
            destroy_buffer_resource(slot.buffer, slot.memory);
            slot = {};
        }
    }
    for (ImageSlot& slot : images) {
        if (slot.state != ResourceState::free) {
            destroy_image_resource(slot);
            slot = {};
        }
    }
    for (SamplerSlot& slot : samplers) {
        if (slot.state != ResourceState::free && slot.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, slot.sampler, nullptr);
            slot.sampler = VK_NULL_HANDLE;
            slot.state = ResourceState::free;
        }
    }
    for (PipelineSlot& slot : pipelines) {
        if (slot.state != ResourceState::free) {
            destroy_pipeline_object(slot);
            slot = {};
        }
    }
    buffers.clear();
    images.clear();
    samplers.clear();
    pipelines.clear();
    for (auto& deletions : deferred_deletions) {
        deletions.clear();
    }
}

void Renderer::Impl::cleanup_swapchain() noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    if (!command_buffers.empty()) {
        vkFreeCommandBuffers(device,
                             command_pool,
                             static_cast<std::uint32_t>(command_buffers.size()),
                             command_buffers.data());
        command_buffers.clear();
    }
    for (VkFramebuffer framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers.clear();
    for (PipelineSlot& slot : pipelines) {
        if (slot.state == ResourceState::live || slot.state == ResourceState::pending) {
            destroy_pipeline_object(slot);
        }
    }
    if (render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
    }
    for (VkImageView image_view : swapchain_image_views) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    swapchain_image_views.clear();
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    swapchain_images.clear();
    images_in_flight.clear();
}

core::Status Renderer::Impl::recreate_swapchain(platform::WindowSize window_size) noexcept
{
    if (window_size.width == 0 || window_size.height == 0) {
        return core::Status{};
    }
    if (vkDeviceWaitIdle(device) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }
    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        collect_deferred(index);
    }
    cleanup_swapchain();
    return create_swapchain(window_size);
}

VkResult Renderer::Impl::record_command_buffer(VkCommandBuffer command_buffer,
                                                core::u32 image_index) noexcept
{
    if (image_index >= framebuffers.size()) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const PipelineSlot* pipeline = nullptr;
    if (triangle_pipeline.valid() && triangle_pipeline.index < pipelines.size()) {
        const PipelineSlot& candidate = pipelines[triangle_pipeline.index];
        if (candidate.state == ResourceState::live &&
            candidate.generation == triangle_pipeline.generation) {
            pipeline = &candidate;
        }
    }
    if (pipeline == nullptr || pipeline->pipeline == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const BufferSlot* vertex_buffer = nullptr;
    if (triangle_buffer.valid() && triangle_buffer.index < buffers.size()) {
        const BufferSlot& candidate = buffers[triangle_buffer.index];
        if (candidate.state == ResourceState::live &&
            candidate.generation == triangle_buffer.generation) {
            vertex_buffer = &candidate;
        }
    }
    if (vertex_buffer == nullptr || vertex_buffer->buffer == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return result;
    }

    VkClearValue clear_value{};
    clear_value.color = {{0.02F, 0.03F, 0.06F, 1.0F}};
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swapchain_extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_extent.width);
    viewport.height = static_cast<float>(swapchain_extent.height);
    viewport.maxDepth = 1.0F;
    VkRect2D scissor{};
    scissor.extent = swapchain_extent;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer->buffer, &offset);
    begin_debug_label(command_buffer, "GameEngine.Triangle");
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    end_debug_label(command_buffer);
    vkCmdEndRenderPass(command_buffer);
    return vkEndCommandBuffer(command_buffer);
}

core::Status Renderer::Impl::render_frame(platform::WindowSize window_size) noexcept
{
    if (window_size.width == 0 || window_size.height == 0) {
        return core::Status{};
    }
    if (window_size.width != last_window_size.width ||
        window_size.height != last_window_size.height) {
        const core::Status status = recreate_swapchain(window_size);
        if (!status) {
            return status;
        }
    }

    if (vkWaitForFences(device,
                        1,
                        &in_flight_fences[current_frame],
                        VK_TRUE,
                        std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }
    collect_deferred(current_frame);

    std::uint32_t image_index = 0;
    const VkResult acquire_result = vkAcquireNextImageKHR(device,
                                                           swapchain,
                                                           std::numeric_limits<std::uint64_t>::max(),
                                                           image_available[current_frame],
                                                           VK_NULL_HANDLE,
                                                           &image_index);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        return recreate_swapchain(window_size);
    }
    if (acquire_result == VK_ERROR_SURFACE_LOST_KHR) {
        return core::Status{core::ErrorCode::vulkan_surface_lost};
    }
    if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }

    if (images_in_flight[image_index] != VK_NULL_HANDLE) {
        if (vkWaitForFences(device,
                            1,
                            &images_in_flight[image_index],
                            VK_TRUE,
                            std::numeric_limits<std::uint64_t>::max()) != VK_SUCCESS) {
            return core::Status{core::ErrorCode::vulkan_frame_failed};
        }
    }
    images_in_flight[image_index] = in_flight_fences[current_frame];

    if (vkResetFences(device, 1, &in_flight_fences[current_frame]) != VK_SUCCESS ||
        vkResetCommandBuffer(command_buffers[image_index], 0) != VK_SUCCESS ||
        record_command_buffer(command_buffers[image_index], image_index) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }

    const VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available[current_frame];
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[image_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished[current_frame];
    if (vkQueueSubmit(graphics_queue,
                      1,
                      &submit_info,
                      in_flight_fences[current_frame]) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished[current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &image_index;
    const VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);
    if (present_result == VK_ERROR_SURFACE_LOST_KHR) {
        return core::Status{core::ErrorCode::vulkan_surface_lost};
    }
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR ||
        acquire_result == VK_SUBOPTIMAL_KHR) {
        const core::Status status = recreate_swapchain(window_size);
        if (!status) {
            return status;
        }
    } else if (present_result != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }

    current_frame = (current_frame + 1) % frames_in_flight;
    return validation_error ? core::Status{core::ErrorCode::vulkan_validation_failed}
                            : core::Status{};
}

void Renderer::Impl::set_debug_name(VkObjectType object_type,
                                    std::uint64_t object,
                                    const char* name) noexcept
{
    if (set_debug_utils_object_name == nullptr || object == 0 || name == nullptr) {
        return;
    }
    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = object_type;
    info.objectHandle = object;
    info.pObjectName = name;
    static_cast<void>(set_debug_utils_object_name(device, &info));
}

void Renderer::Impl::begin_debug_label(VkCommandBuffer command_buffer, const char* name) noexcept
{
    if (cmd_begin_debug_utils_label == nullptr || name == nullptr) {
        return;
    }
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = 0.2F;
    label.color[1] = 0.6F;
    label.color[2] = 1.0F;
    label.color[3] = 1.0F;
    cmd_begin_debug_utils_label(command_buffer, &label);
}

void Renderer::Impl::end_debug_label(VkCommandBuffer command_buffer) noexcept
{
    if (cmd_end_debug_utils_label != nullptr) {
        cmd_end_debug_utils_label(command_buffer);
    }
}

Renderer::~Renderer() noexcept
{
    shutdown();
}

core::Status Renderer::initialize(const platform::Platform& platform) noexcept
{
    if (initialized_) {
        return core::Status{core::ErrorCode::already_initialized};
    }
    if (!platform.is_initialized() || !platform.has_window()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    impl_ = new (std::nothrow) Impl{};
    if (impl_ == nullptr) {
        return core::Status{core::ErrorCode::allocation_failed};
    }

    const core::Status status = impl_->initialize(platform.native_window_handles(),
                                                  platform.window_size());
    if (!status) {
        impl_->shutdown();
        delete impl_;
        impl_ = nullptr;
        return status;
    }
    initialized_ = true;
    return core::Status{};
}

core::Status Renderer::render_frame(const platform::Platform& platform) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!platform.is_initialized() || !platform.has_window()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return impl_->render_frame(platform.window_size());
}

core::Status Renderer::create_buffer(const BufferDescription& description,
                                     BufferHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->create_buffer(description, handle);
}

core::Status Renderer::upload_buffer(BufferHandle handle,
                                     std::span<const std::byte> data) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->upload_buffer(handle, data);
}

core::Status Renderer::destroy_buffer(BufferHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->destroy_buffer(handle);
}

core::Status Renderer::create_image(const ImageDescription& description,
                                    ImageHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->create_image(description, handle);
}

core::Status Renderer::upload_image(ImageHandle handle,
                                    std::span<const std::byte> data) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->upload_image(handle, data);
}

core::Status Renderer::destroy_image(ImageHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->destroy_image(handle);
}

core::Status Renderer::create_sampler(const SamplerDescription& description,
                                      SamplerHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->create_sampler(description, handle);
}

core::Status Renderer::destroy_sampler(SamplerHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->destroy_sampler(handle);
}

core::Status Renderer::create_graphics_pipeline(
    const GraphicsPipelineDescription& description,
    PipelineHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->create_graphics_pipeline(description, handle);
}

core::Status Renderer::destroy_pipeline(PipelineHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->destroy_pipeline(handle);
}

void Renderer::shutdown() noexcept
{
    if (impl_ != nullptr) {
        impl_->shutdown();
        delete impl_;
        impl_ = nullptr;
    }
    initialized_ = false;
}

} // namespace gameengine::rhi
