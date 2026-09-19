#include "engine/rhi/rhi.hpp"

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

#include "engine/core/diagnostics.hpp"
#include "engine/math/math.hpp"
#include "engine/renderer/gpu_culling.hpp"
#include "engine/renderer/render_graph/render_graph.hpp"
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/procedural_instances.hpp"
#include "engine/renderer/vulkan/shader_pipeline.hpp"
#include "engine/renderer/vulkan/triangle_shaders.hpp"
#include "engine/renderer/vulkan/vulkan_pipeline_cache.hpp"
#include "engine/renderer/vulkan/vulkan_utils.hpp"
#include "engine/scene/bootstrap_cube.hpp"
#include "engine/scene/bootstrap_material.hpp"
#include "engine/scene/scene.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace gameengine::rhi {

namespace {

constexpr std::array<const char*, 1> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
constexpr core::u32 frames_in_flight = 2;
constexpr core::u32 timestamp_queries_per_frame = renderer::metrics::max_timed_passes * 2U;
constexpr core::usize pipeline_cache_path_capacity = 512;

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

struct ViewProjectionPushConstants final {
    math::Mat4 view_projection{};
};

static_assert(sizeof(ViewProjectionPushConstants) == 64U);
static_assert(sizeof(ViewProjectionPushConstants) ==
              renderer::vulkan::bootstrap::shader_push_constant_size);

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
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    VkImage depth_image = VK_NULL_HANDLE;
    VkDeviceMemory depth_memory = VK_NULL_HANDLE;
    VkImageView depth_image_view = VK_NULL_HANDLE;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout material_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool material_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet material_descriptor_set = VK_NULL_HANDLE;
    VkBuffer material_uniform_buffer = VK_NULL_HANDLE;
    VkDeviceMemory material_uniform_memory = VK_NULL_HANDLE;
    VkBuffer instance_buffer = VK_NULL_HANDLE;
    VkDeviceMemory instance_memory = VK_NULL_HANDLE;
    void* instance_mapped = nullptr;
    VkDeviceSize instance_slice_stride = 0;
    VkDeviceSize instance_buffer_size = 0;
    VkDeviceSize instance_non_coherent_atom_size = 1;
    VkMemoryPropertyFlags instance_memory_properties = 0;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    VkPipeline gpu_cull_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout gpu_cull_pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout gpu_cull_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool gpu_cull_descriptor_pool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, frames_in_flight> gpu_cull_descriptor_sets{};
    VkBuffer gpu_source_buffer = VK_NULL_HANDLE;
    VkDeviceMemory gpu_source_memory = VK_NULL_HANDLE;
    void* gpu_source_mapped = nullptr;
    VkDeviceSize gpu_source_buffer_size = 0;
    VkMemoryPropertyFlags gpu_source_memory_properties = 0;
    VkBuffer gpu_visible_buffer = VK_NULL_HANDLE;
    VkDeviceMemory gpu_visible_memory = VK_NULL_HANDLE;
    VkDeviceSize gpu_visible_slice_stride = 0;
    VkDeviceSize gpu_visible_buffer_size = 0;
    VkBuffer gpu_indirect_buffer = VK_NULL_HANDLE;
    VkDeviceMemory gpu_indirect_memory = VK_NULL_HANDLE;
    void* gpu_indirect_mapped = nullptr;
    VkDeviceSize gpu_indirect_slice_stride = 0;
    VkDeviceSize gpu_indirect_buffer_size = 0;
    VkMemoryPropertyFlags gpu_indirect_memory_properties = 0;
    renderer::vulkan::PipelineCacheIdentity pipeline_cache_identity{};
    VkQueryPool timestamp_query_pool = VK_NULL_HANDLE;
    PFN_vkResetQueryPool reset_query_pool = nullptr;
    core::f64 timestamp_period = 0.0;
    core::u32 timestamp_valid_bits = 0;
    bool gpu_timestamps_enabled = false;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkCommandBuffer> command_buffers;

    std::array<VkSemaphore, frames_in_flight> image_available{};
    std::vector<VkSemaphore> render_finished;
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
    std::array<char, pipeline_cache_path_capacity> pipeline_cache_path{};
    renderer::vulkan::ShaderDeviceCapabilities shader_capabilities{};
    const renderer::vulkan::ShaderArtifact* vertex_shader_artifact = nullptr;
    const renderer::vulkan::ShaderArtifact* fragment_shader_artifact = nullptr;
    const renderer::vulkan::ShaderArtifact* compute_shader_artifact = nullptr;
    bool shader_hot_reload_enabled = false;
    scene::Scene bootstrap_scene;
    scene::Entity bootstrap_cube_entity{};
    scene::Entity bootstrap_camera_entity{};
    scene::Entity bootstrap_light_entity{};
    renderer::render_graph::RenderGraph render_graph;
    renderer::render_graph::ResourceHandle swapchain_color_resource{};
    renderer::render_graph::ResourceHandle depth_resource{};
    renderer::render_graph::ResourceHandle gpu_source_resource{};
    renderer::render_graph::ResourceHandle gpu_visible_resource{};
    renderer::render_graph::ResourceHandle gpu_indirect_resource{};
    renderer::render_graph::PassHandle gpu_cull_pass{};
    renderer::render_graph::PassHandle forward_opaque_pass{};
    std::array<renderer::metrics::FrameTimingReport, frames_in_flight> frame_timing{};
    std::array<bool, frames_in_flight> timing_pending{};
    renderer::metrics::TimingAccumulator timing_accumulator{};
    std::vector<renderer::procedural::ProceduralInstance> procedural_instances;
    core::u32 active_instance_count = renderer::procedural::default_instance_count;
    renderer::gpu_culling::VisibilityMode visibility_mode =
        renderer::gpu_culling::VisibilityMode::cpu;
    bool gpu_culling_available = false;
    bool gpu_culling_fallback = false;
    rhi::BufferHandle cube_vertex_buffer{};
    rhi::BufferHandle cube_index_buffer{};
    rhi::PipelineHandle cube_pipeline{};
    rhi::ImageHandle bootstrap_albedo_image{};
    rhi::SamplerHandle bootstrap_albedo_sampler{};
    PFN_vkSetDebugUtilsObjectNameEXT set_debug_utils_object_name = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_debug_utils_label = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_debug_utils_label = nullptr;
    bool validation_error = false;

    [[nodiscard]] core::Status initialize(platform::NativeWindowHandles handles,
                                           platform::WindowSize window_size,
                                           const rhi::RendererConfiguration& configuration) noexcept;
    void shutdown() noexcept;
    [[nodiscard]] core::Status render_frame(platform::WindowSize window_size) noexcept;
    [[nodiscard]] core::Status reload_shaders() noexcept;

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
    [[nodiscard]] core::Status create_pipeline_cache() noexcept;
    void persist_pipeline_cache() noexcept;
    [[nodiscard]] core::Status select_shader_variants() noexcept;
    [[nodiscard]] core::Status create_timing_resources() noexcept;
    void destroy_timing_resources() noexcept;
    [[nodiscard]] core::Status create_render_graph() noexcept;
    void reset_timing_queries(core::u32 frame_index) noexcept;
    void resolve_timing(core::u32 frame_index) noexcept;
    void resolve_all_timing() noexcept;
    [[nodiscard]] core::Status create_command_pool() noexcept;
    [[nodiscard]] core::Status recreate_swapchain(platform::WindowSize window_size) noexcept;
    [[nodiscard]] core::Status create_swapchain(platform::WindowSize window_size) noexcept;
    [[nodiscard]] core::Status create_image_views() noexcept;
    [[nodiscard]] VkFormat find_depth_format() const noexcept;
    [[nodiscard]] core::Status create_depth_resources() noexcept;
    [[nodiscard]] core::Status create_render_pass() noexcept;
    [[nodiscard]] core::Status create_framebuffers() noexcept;
    [[nodiscard]] core::Status create_command_buffers() noexcept;
    [[nodiscard]] core::Status create_render_finished_semaphores() noexcept;
    void destroy_render_finished_semaphores() noexcept;
    [[nodiscard]] core::Status create_sync_objects() noexcept;
    [[nodiscard]] core::Status create_bootstrap_cube_resources() noexcept;
    [[nodiscard]] core::Status create_bootstrap_scene() noexcept;
    [[nodiscard]] core::Status create_procedural_instance_resources() noexcept;
    void destroy_procedural_instance_resources() noexcept;
    [[nodiscard]] core::Status set_procedural_workload(core::u32 instance_count) noexcept;
    [[nodiscard]] core::Status create_gpu_culling_resources() noexcept;
    void destroy_gpu_culling_resources() noexcept;
    [[nodiscard]] core::Status set_visibility_mode(
        renderer::gpu_culling::VisibilityMode mode) noexcept;
    [[nodiscard]] core::Status upload_gpu_source_instances() noexcept;
    [[nodiscard]] core::Status create_gpu_culling_pipeline() noexcept;
    [[nodiscard]] core::Status create_gpu_culling_descriptors() noexcept;
    void destroy_gpu_culling_pipeline() noexcept;
    void resolve_gpu_visibility(core::u32 frame_index) noexcept;
    [[nodiscard]] core::Status create_bootstrap_material_resources() noexcept;
    [[nodiscard]] core::Status create_material_descriptors() noexcept;
    void destroy_material_resources() noexcept;
    [[nodiscard]] core::Status rebuild_pipelines() noexcept;
    [[nodiscard]] core::Status build_pipeline_object(const PipelineSlot& slot,
                                                     VkPipeline& pipeline) noexcept;
    [[nodiscard]] VkResult record_command_buffer(VkCommandBuffer command_buffer,
                                                  core::u32 image_index,
                                                  core::u32 frame_index) noexcept;

    void cleanup_swapchain() noexcept;
    void collect_deferred(core::u32 frame_index) noexcept;
    void destroy_live_resources() noexcept;
    void release_deferred(const DeferredDeletion& deletion) noexcept;
    [[nodiscard]] core::Status create_buffer_resource(VkDeviceSize size,
                                                       VkBufferUsageFlags usage,
                                                       VkMemoryPropertyFlags properties,
                                                       VkBuffer& buffer,
                                                       VkDeviceMemory& memory,
                                                       VkMemoryPropertyFlags* allocated_properties =
                                                           nullptr) noexcept;
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

core::Status Renderer::Impl::initialize(
    platform::NativeWindowHandles handles,
    platform::WindowSize window_size,
    const rhi::RendererConfiguration& configuration) noexcept
{
    if (!handles.valid() || window_size.width == 0 || window_size.height == 0) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (configuration.pipeline_cache_path == nullptr) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const std::size_t cache_path_length = std::strlen(configuration.pipeline_cache_path);
    if (cache_path_length == 0 || cache_path_length >= pipeline_cache_path.size()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
#ifdef NDEBUG
    if (configuration.enable_shader_hot_reload) {
        return core::Status{core::ErrorCode::shader_reload_disabled};
    }
#endif
    std::memcpy(pipeline_cache_path.data(),
                configuration.pipeline_cache_path,
                cache_path_length + 1);
    shader_hot_reload_enabled = configuration.enable_shader_hot_reload;
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
    status = select_shader_variants();
    if (!status) {
        return status;
    }
    status = create_pipeline_cache();
    if (!status) {
        return status;
    }
    status = create_command_pool();
    if (!status) {
        return status;
    }
    status = create_timing_resources();
    if (!status) {
        return status;
    }
    status = create_bootstrap_scene();
    if (!status) {
        return status;
    }
    status = create_procedural_instance_resources();
    if (!status) {
        return status;
    }
    status = create_gpu_culling_resources();
    if (!status) {
        return status;
    }
    status = create_bootstrap_cube_resources();
    if (!status) {
        return status;
    }
    status = create_bootstrap_material_resources();
    if (!status) {
        return status;
    }
    status = create_material_descriptors();
    if (!status) {
        return status;
    }
    status = create_swapchain(window_size);
    if (!status) {
        return status;
    }
    status = create_render_graph();
    if (!status) {
        return status;
    }
    status = create_sync_objects();
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

    resolve_all_timing();
    persist_pipeline_cache();

    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        collect_deferred(index);
    }

    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        if (image_available[index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, image_available[index], nullptr);
        }
        if (in_flight_fences[index] != VK_NULL_HANDLE) {
            vkDestroyFence(device, in_flight_fences[index], nullptr);
        }
    }

    cleanup_swapchain();
    destroy_gpu_culling_resources();
    destroy_procedural_instance_resources();
    destroy_timing_resources();
    destroy_material_resources();
    destroy_live_resources();

    if (device != VK_NULL_HANDLE && pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }

    if (device != VK_NULL_HANDLE && pipeline_cache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(device, pipeline_cache, nullptr);
        pipeline_cache = VK_NULL_HANDLE;
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
    pipeline_cache_path = {};
    pipeline_cache_identity = {};
    gpu_cull_pipeline = VK_NULL_HANDLE;
    gpu_cull_pipeline_layout = VK_NULL_HANDLE;
    gpu_cull_descriptor_set_layout = VK_NULL_HANDLE;
    gpu_cull_descriptor_pool = VK_NULL_HANDLE;
    gpu_cull_descriptor_sets = {};
    gpu_source_buffer = VK_NULL_HANDLE;
    gpu_source_memory = VK_NULL_HANDLE;
    gpu_source_mapped = nullptr;
    gpu_source_buffer_size = 0;
    gpu_source_memory_properties = 0;
    gpu_visible_buffer = VK_NULL_HANDLE;
    gpu_visible_memory = VK_NULL_HANDLE;
    gpu_visible_slice_stride = 0;
    gpu_visible_buffer_size = 0;
    gpu_indirect_buffer = VK_NULL_HANDLE;
    gpu_indirect_memory = VK_NULL_HANDLE;
    gpu_indirect_mapped = nullptr;
    gpu_indirect_slice_stride = 0;
    gpu_indirect_buffer_size = 0;
    gpu_indirect_memory_properties = 0;
    reset_query_pool = nullptr;
    timestamp_period = 0.0;
    timestamp_valid_bits = 0;
    gpu_timestamps_enabled = false;
    render_graph.reset();
    swapchain_color_resource = {};
    depth_resource = {};
    gpu_source_resource = {};
    gpu_visible_resource = {};
    gpu_indirect_resource = {};
    gpu_cull_pass = {};
    forward_opaque_pass = {};
    frame_timing = {};
    timing_pending = {};
    timing_accumulator.reset();
    instance_buffer = VK_NULL_HANDLE;
    instance_memory = VK_NULL_HANDLE;
    instance_mapped = nullptr;
    instance_slice_stride = 0;
    instance_buffer_size = 0;
    instance_non_coherent_atom_size = 1;
    instance_memory_properties = 0;
    procedural_instances.clear();
    active_instance_count = renderer::procedural::default_instance_count;
    visibility_mode = renderer::gpu_culling::VisibilityMode::cpu;
    gpu_culling_available = false;
    gpu_culling_fallback = false;
    shader_capabilities = {};
    vertex_shader_artifact = nullptr;
    fragment_shader_artifact = nullptr;
    compute_shader_artifact = nullptr;
    shader_hot_reload_enabled = false;
    bootstrap_scene.clear();
    bootstrap_cube_entity = {};
    bootstrap_camera_entity = {};
    bootstrap_light_entity = {};
    cube_vertex_buffer = {};
    cube_index_buffer = {};
    cube_pipeline = {};
    bootstrap_albedo_image = {};
    bootstrap_albedo_sampler = {};
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
#if defined(_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
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
#if defined(_WIN32)
    const auto hinstance = reinterpret_cast<HINSTANCE>(native_handles.display);
    const auto hwnd = reinterpret_cast<HWND>(native_handles.window);
    VkWin32SurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hinstance = hinstance;
    create_info.hwnd = hwnd;
    if (vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_surface_failed};
    }
    return core::Status{};
#else
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
#endif
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

core::Status Renderer::Impl::select_shader_variants() noexcept
{
    shader_capabilities.supported_capabilities =
        renderer::vulkan::shader_capability_vulkan_1_0;

    const auto vertex = renderer::vulkan::select_shader_variant(
        renderer::vulkan::bootstrap::vertex_shader_variants,
        shader_capabilities);
    const auto fragment = renderer::vulkan::select_shader_variant(
        renderer::vulkan::bootstrap::fragment_shader_variants,
        shader_capabilities);
    const auto compute = renderer::vulkan::select_shader_variant(
        renderer::vulkan::bootstrap::compute_shader_variants,
        shader_capabilities);
    if (vertex == nullptr || fragment == nullptr || compute == nullptr) {
        return core::Status{core::ErrorCode::shader_variant_unavailable};
    }
    vertex_shader_artifact = vertex;
    fragment_shader_artifact = fragment;
    compute_shader_artifact = compute;
    return core::Status{};
}

core::Status Renderer::Impl::create_timing_resources() noexcept
{
    gpu_timestamps_enabled = false;
    reset_query_pool = nullptr;
    timestamp_period = 0.0;
    timestamp_valid_bits = 0;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    const QueueFamilies families = find_queue_families(physical_device);
    if (!families.graphics.has_value() || properties.limits.timestampPeriod <= 0.0F) {
        return core::Status{};
    }

    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> family_properties(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &family_count, family_properties.data());
    if (families.graphics.value() >= family_properties.size() ||
        family_properties[families.graphics.value()].timestampValidBits == 0U) {
        return core::Status{};
    }

    reset_query_pool = reinterpret_cast<PFN_vkResetQueryPool>(
        vkGetDeviceProcAddr(device, "vkResetQueryPool"));
    if (reset_query_pool == nullptr) {
        return core::Status{};
    }

    VkQueryPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    create_info.queryCount = frames_in_flight * timestamp_queries_per_frame;
    if (vkCreateQueryPool(device, &create_info, nullptr, &timestamp_query_pool) != VK_SUCCESS) {
        reset_query_pool = nullptr;
        return core::Status{};
    }

    timestamp_period = static_cast<core::f64>(properties.limits.timestampPeriod);
    timestamp_valid_bits = family_properties[families.graphics.value()].timestampValidBits;
    gpu_timestamps_enabled = true;
    return core::Status{};
}

void Renderer::Impl::destroy_timing_resources() noexcept
{
    if (device != VK_NULL_HANDLE && timestamp_query_pool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device, timestamp_query_pool, nullptr);
    }
    timestamp_query_pool = VK_NULL_HANDLE;
    reset_query_pool = nullptr;
    timestamp_period = 0.0;
    timestamp_valid_bits = 0;
    gpu_timestamps_enabled = false;
}

core::Status Renderer::Impl::create_render_graph() noexcept
{
    render_graph.reset();
    gpu_source_resource = {};
    gpu_visible_resource = {};
    gpu_indirect_resource = {};
    gpu_cull_pass = {};
    if (!render_graph
             .add_resource({"swapchain_color", renderer::render_graph::ResourceKind::color_attachment,
                            true},
                           swapchain_color_resource)
             .ok() ||
        !render_graph
             .add_resource({"depth_attachment", renderer::render_graph::ResourceKind::depth_attachment,
                            true},
                           depth_resource)
             .ok()) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    std::array<renderer::render_graph::ResourceHandle, 2> forward_reads{};
    core::u32 forward_read_count = 0U;
    if (visibility_mode == renderer::gpu_culling::VisibilityMode::gpu &&
        gpu_culling_available) {
        if (!render_graph
                 .add_resource({"instance_source",
                                renderer::render_graph::ResourceKind::storage_buffer,
                                true},
                               gpu_source_resource)
                 .ok() ||
            !render_graph
                 .add_resource({"visible_instances",
                                renderer::render_graph::ResourceKind::vertex_buffer,
                                true},
                               gpu_visible_resource)
                 .ok() ||
            !render_graph
                 .add_resource({"indirect_command",
                                renderer::render_graph::ResourceKind::indirect_buffer,
                                true},
                               gpu_indirect_resource)
                 .ok()) {
            return core::Status{core::ErrorCode::vulkan_swapchain_failed};
        }
        const std::array<renderer::render_graph::ResourceHandle, 1> cull_reads = {
            gpu_source_resource,
        };
        const std::array<renderer::render_graph::ResourceHandle, 2> cull_writes = {
            gpu_visible_resource,
            gpu_indirect_resource,
        };
        const renderer::render_graph::PassDescription cull_pass{
            .name = "gpu_cull",
            .reads = cull_reads,
            .writes = cull_writes,
            .dependencies = {},
            .draw_calls = 0U,
        };
        if (!render_graph.add_pass(cull_pass, gpu_cull_pass).ok()) {
            return core::Status{core::ErrorCode::vulkan_swapchain_failed};
        }
        forward_reads = {gpu_visible_resource, gpu_indirect_resource};
        forward_read_count = 2U;
    }

    const std::array<renderer::render_graph::ResourceHandle, 2> writes = {
        swapchain_color_resource,
        depth_resource,
    };
    const renderer::render_graph::PassDescription forward_pass{
        .name = "forward_opaque",
        .reads = std::span<const renderer::render_graph::ResourceHandle>{forward_reads.data(),
                                                                         forward_read_count},
        .writes = writes,
        .dependencies = {},
        .draw_calls = 1U,
    };
    if (!render_graph.add_pass(forward_pass, forward_opaque_pass).ok() ||
        !render_graph.compile().ok()) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }
    return core::Status{};
}

void Renderer::Impl::reset_timing_queries(core::u32 frame_index) noexcept
{
    if (!gpu_timestamps_enabled || reset_query_pool == nullptr ||
        frame_index >= frames_in_flight) {
        return;
    }
    reset_query_pool(device,
                    timestamp_query_pool,
                    frame_index * timestamp_queries_per_frame,
                    timestamp_queries_per_frame);
}

void Renderer::Impl::resolve_timing(core::u32 frame_index) noexcept
{
    if (frame_index >= frames_in_flight || !timing_pending[frame_index]) {
        return;
    }

    renderer::metrics::FrameTimingReport& report = frame_timing[frame_index];
    if (gpu_timestamps_enabled && report.pass_count > 0U) {
        std::array<std::uint64_t, timestamp_queries_per_frame> timestamps{};
        const VkResult result = vkGetQueryPoolResults(device,
                                                       timestamp_query_pool,
                                                       frame_index * timestamp_queries_per_frame,
                                                       report.pass_count * 2U,
                                                       sizeof(timestamps),
                                                       timestamps.data(),
                                                       sizeof(std::uint64_t),
                                                       VK_QUERY_RESULT_64_BIT);
        if (result == VK_SUCCESS) {
            for (core::u32 index = 0; index < report.pass_count; ++index) {
                renderer::metrics::PassTiming& pass = report.passes[index];
                pass.gpu_nanoseconds = renderer::metrics::timestamp_delta_to_nanoseconds(
                    timestamps[index * 2U],
                    timestamps[index * 2U + 1U],
                    timestamp_period,
                    timestamp_valid_bits);
                pass.gpu_time_valid = true;
            }
        } else {
            report.gpu_timestamps_available = false;
            for (core::u32 index = 0; index < report.pass_count; ++index) {
                report.passes[index].gpu_time_valid = false;
            }
        }
    }

    timing_accumulator.record(report);
    timing_pending[frame_index] = false;
}

void Renderer::Impl::resolve_all_timing() noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        resolve_gpu_visibility(index);
        resolve_timing(index);
    }
}

void Renderer::Impl::resolve_gpu_visibility(core::u32 frame_index) noexcept
{
    if (visibility_mode != renderer::gpu_culling::VisibilityMode::gpu ||
        !gpu_culling_available || frame_index >= frames_in_flight ||
        !timing_pending[frame_index] || gpu_indirect_mapped == nullptr) {
        return;
    }
    if ((gpu_indirect_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0U) {
        const VkMappedMemoryRange range{
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = gpu_indirect_memory,
            .offset = gpu_indirect_slice_stride * frame_index,
            .size = gpu_indirect_slice_stride,
        };
        if (vkInvalidateMappedMemoryRanges(device, 1, &range) != VK_SUCCESS) {
            return;
        }
    }
    const auto* command = reinterpret_cast<const renderer::gpu_culling::IndirectCommand*>(
        static_cast<const std::byte*>(gpu_indirect_mapped) +
        gpu_indirect_slice_stride * frame_index);
    auto& report = frame_timing[frame_index];
    report.visible_instances = std::min(command->instance_count, report.total_instances);
    report.culled_instances = report.total_instances - report.visible_instances;
}

core::Status Renderer::Impl::create_pipeline_cache() noexcept
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    pipeline_cache_identity = renderer::vulkan::make_pipeline_cache_identity(
        properties,
        VK_API_VERSION_1_0);

    std::vector<std::byte> initial_data;
    const auto load_result = renderer::vulkan::load_pipeline_cache_file(
        pipeline_cache_path.data(),
        pipeline_cache_identity,
        initial_data);
    if (load_result == renderer::vulkan::PipelineCacheLoadResult::ignored) {
        core::log(core::LogLevel::warning, "Ignoring incompatible Vulkan pipeline cache");
    }

    VkPipelineCacheCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    create_info.initialDataSize = initial_data.size();
    create_info.pInitialData = initial_data.empty() ? nullptr : initial_data.data();
    VkResult result = vkCreatePipelineCache(device, &create_info, nullptr, &pipeline_cache);
    if (result != VK_SUCCESS && !initial_data.empty()) {
        core::log(core::LogLevel::warning,
                  "Vulkan pipeline cache rejected its payload; starting empty");
        create_info.initialDataSize = 0;
        create_info.pInitialData = nullptr;
        result = vkCreatePipelineCache(device, &create_info, nullptr, &pipeline_cache);
    }
    if (result != VK_SUCCESS) {
        core::log(core::LogLevel::error, "Unable to create Vulkan pipeline cache");
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    return core::Status{};
}

void Renderer::Impl::persist_pipeline_cache() noexcept
{
    if (device == VK_NULL_HANDLE || pipeline_cache == VK_NULL_HANDLE ||
        pipeline_cache_path[0] == '\0') {
        return;
    }

    std::size_t data_size = 0;
    if (vkGetPipelineCacheData(device, pipeline_cache, &data_size, nullptr) != VK_SUCCESS ||
        data_size > renderer::vulkan::pipeline_cache_max_payload) {
        core::log(core::LogLevel::warning, "Unable to query Vulkan pipeline cache data");
        return;
    }
    std::vector<std::byte> data(data_size);
    if (!data.empty() &&
        vkGetPipelineCacheData(device, pipeline_cache, &data_size, data.data()) != VK_SUCCESS) {
        core::log(core::LogLevel::warning, "Unable to read Vulkan pipeline cache data");
        return;
    }
    data.resize(data_size);
    if (!renderer::vulkan::write_pipeline_cache_file(
            pipeline_cache_path.data(), pipeline_cache_identity, data)) {
        core::log(core::LogLevel::warning, "Unable to persist Vulkan pipeline cache");
    }
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
    status = create_depth_resources();
    if (!status) {
        return status;
    }
    status = create_render_pass();
    if (!status) {
        return status;
    }
    if (cube_pipeline.valid()) {
        status = rebuild_pipelines();
    } else {
        rhi::GraphicsPipelineDescription description{};
        status = create_graphics_pipeline(description, cube_pipeline);
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
    status = create_render_finished_semaphores();
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

VkFormat Renderer::Impl::find_depth_format() const noexcept
{
    constexpr std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };
    for (const VkFormat candidate : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physical_device, candidate, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) !=
            0U) {
            return candidate;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

core::Status Renderer::Impl::create_depth_resources() noexcept
{
    depth_format = find_depth_format();
    if (depth_format == VK_FORMAT_UNDEFINED) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = depth_format;
    image_info.extent = {swapchain_extent.width, swapchain_extent.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &image_info, nullptr, &depth_image) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, depth_image, &requirements);
    std::uint32_t memory_type = 0;
    core::Status status = find_memory_type(requirements.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                           memory_type);
    if (!status) {
        return status;
    }

    VkMemoryAllocateInfo allocation_info{};
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = requirements.size;
    allocation_info.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(device, &allocation_info, nullptr, &depth_memory) != VK_SUCCESS ||
        vkBindImageMemory(device, depth_image, depth_memory, 0) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = depth_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = depth_format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &view_info, nullptr, &depth_image_view) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }
    set_debug_name(VK_OBJECT_TYPE_IMAGE,
                   reinterpret_cast<std::uint64_t>(depth_image),
                   "GameEngine.DepthImage");
    set_debug_name(VK_OBJECT_TYPE_IMAGE_VIEW,
                   reinterpret_cast<std::uint64_t>(depth_image_view),
                   "GameEngine.DepthImageView");
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

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_reference{};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference{};
    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    subpass.pDepthStencilAttachment = &depth_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const std::array<VkAttachmentDescription, 2> attachments = {
        color_attachment,
        depth_attachment,
    };

    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    create_info.pAttachments = attachments.data();
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

core::Status Renderer::Impl::build_pipeline_object(const PipelineSlot& slot,
                                                    VkPipeline& pipeline) noexcept
{
    pipeline = VK_NULL_HANDLE;
    if (slot.description.vertex_layout != rhi::PipelineVertexLayout::position3_color3) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (vertex_shader_artifact == nullptr || fragment_shader_artifact == nullptr ||
        pipeline_cache == VK_NULL_HANDLE) {
        return core::Status{core::ErrorCode::shader_variant_unavailable};
    }

    const VkShaderModule vertex_shader = create_shader_module(
        vertex_shader_artifact->spirv,
        vertex_shader_artifact->spirv_word_count * sizeof(std::uint32_t));
    const VkShaderModule fragment_shader = create_shader_module(
        fragment_shader_artifact->spirv,
        fragment_shader_artifact->spirv_word_count * sizeof(std::uint32_t));
    if (vertex_shader == VK_NULL_HANDLE || fragment_shader == VK_NULL_HANDLE) {
        if (vertex_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertex_shader, nullptr);
        }
        if (fragment_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragment_shader, nullptr);
        }
        return core::Status{core::ErrorCode::vulkan_swapchain_failed};
    }

    const std::array<VkVertexInputBindingDescription, 2> vertex_bindings = {
        VkVertexInputBindingDescription{0,
                                        sizeof(scene::TexturedVertex),
                                        VK_VERTEX_INPUT_RATE_VERTEX},
        VkVertexInputBindingDescription{1,
                                        sizeof(renderer::procedural::InstanceData),
                                        VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    const std::array<VkVertexInputAttributeDescription, 7> vertex_attributes = {
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        VkVertexInputAttributeDescription{1,
                                          0,
                                          VK_FORMAT_R32G32B32_SFLOAT,
                                          sizeof(math::Vec3)},
        VkVertexInputAttributeDescription{2,
                                          0,
                                          VK_FORMAT_R32G32_SFLOAT,
                                          sizeof(math::Vec3) * 2U},
        VkVertexInputAttributeDescription{3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        VkVertexInputAttributeDescription{4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float) * 4U},
        VkVertexInputAttributeDescription{5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float) * 8U},
        VkVertexInputAttributeDescription{6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float) * 12U},
    };
    VkPipelineShaderStageCreateInfo vertex_stage{};
    vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage.module = vertex_shader;
    vertex_stage.pName = vertex_shader_artifact->entry_point.data();
    VkPipelineShaderStageCreateInfo fragment_stage{};
    fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_stage.module = fragment_shader;
    fragment_stage.pName = fragment_shader_artifact->entry_point.data();
    const std::array<VkPipelineShaderStageCreateInfo, 2> stages = {vertex_stage, fragment_stage};

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount =
        static_cast<std::uint32_t>(vertex_bindings.size());
    vertex_input.pVertexBindingDescriptions = vertex_bindings.data();
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

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

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
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(ViewProjectionPushConstants);
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &material_descriptor_set_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant_range;
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
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    if (vkCreateGraphicsPipelines(device,
                                  pipeline_cache,
                                  1,
                                  &pipeline_info,
                                  nullptr,
                                  &pipeline) != VK_SUCCESS) {
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
                   reinterpret_cast<std::uint64_t>(pipeline),
                   "GameEngine.CubePipeline");
    return core::Status{};
}

core::Status Renderer::Impl::create_pipeline_object(PipelineSlot& slot) noexcept
{
    VkPipeline replacement = VK_NULL_HANDLE;
    const core::Status status = build_pipeline_object(slot, replacement);
    if (!status) {
        return status;
    }
    destroy_pipeline_object(slot);
    slot.pipeline = replacement;
    return core::Status{};
}

core::Status Renderer::Impl::create_framebuffers() noexcept
{
    framebuffers.resize(swapchain_image_views.size());
    for (std::size_t index = 0; index < swapchain_image_views.size(); ++index) {
        const std::array<VkImageView, 2> attachments = {
            swapchain_image_views[index],
            depth_image_view,
        };
        VkFramebufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = render_pass;
        create_info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        create_info.pAttachments = attachments.data();
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

core::Status Renderer::Impl::create_render_finished_semaphores() noexcept
{
    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    render_finished.resize(swapchain_images.size(), VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : render_finished) {
        if (vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS) {
            destroy_render_finished_semaphores();
            return core::Status{core::ErrorCode::vulkan_device_failed};
        }
    }
    return core::Status{};
}

void Renderer::Impl::destroy_render_finished_semaphores() noexcept
{
    if (device != VK_NULL_HANDLE) {
        for (VkSemaphore semaphore : render_finished) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(device, semaphore, nullptr);
            }
        }
    }
    render_finished.clear();
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
            vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[index]) != VK_SUCCESS) {
            return core::Status{core::ErrorCode::vulkan_device_failed};
        }
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_bootstrap_cube_resources() noexcept
{
    const rhi::BufferDescription vertex_description{
        .size = sizeof(scene::bootstrap_cube_vertices),
        .usage = rhi::BufferUsage::vertex,
    };
    core::Status status = create_buffer(vertex_description, cube_vertex_buffer);
    if (!status) {
        return status;
    }

    const auto vertex_bytes = std::as_bytes(
        std::span<const scene::TexturedVertex>{scene::bootstrap_cube_vertices});
    status = upload_buffer(cube_vertex_buffer, vertex_bytes);
    if (!status) {
        return status;
    }

    const rhi::BufferDescription index_description{
        .size = sizeof(scene::bootstrap_cube_indices),
        .usage = rhi::BufferUsage::index,
    };
    status = create_buffer(index_description, cube_index_buffer);
    if (!status) {
        return status;
    }

    const auto index_bytes = std::as_bytes(
        std::span<const std::uint16_t>{scene::bootstrap_cube_indices});
    status = upload_buffer(cube_index_buffer, index_bytes);
    if (!status) {
        return status;
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_bootstrap_scene() noexcept
{
    bootstrap_scene.clear();
    bootstrap_scene.reserve(3U);
    bootstrap_cube_entity = bootstrap_scene.create_entity();
    bootstrap_camera_entity = bootstrap_scene.create_entity();
    bootstrap_light_entity = bootstrap_scene.create_entity();
    if (!bootstrap_cube_entity.valid() || !bootstrap_camera_entity.valid() ||
        !bootstrap_light_entity.valid()) {
        return core::Status{core::ErrorCode::allocation_failed};
    }

    const math::Quaternion yaw = math::quaternion_from_axis_angle({0.0F, 1.0F, 0.0F}, 0.65F);
    const math::Quaternion pitch =
        math::quaternion_from_axis_angle({1.0F, 0.0F, 0.0F}, -0.4F);
    if (!bootstrap_scene.add_transform(bootstrap_cube_entity,
                                       {.local_rotation = math::normalize(math::multiply(yaw, pitch))}) ||
        !bootstrap_scene.add_mesh_renderer(
            bootstrap_cube_entity,
            {scene::bootstrap_mesh_id, scene::bootstrap_material_id}) ||
        !bootstrap_scene.add_transform(
            bootstrap_camera_entity,
            {.local_position = {2.5F, 2.0F, 4.0F}}) ||
        !bootstrap_scene.add_camera(bootstrap_camera_entity, {.active = true}) ||
        !bootstrap_scene.add_transform(bootstrap_light_entity) ||
        !bootstrap_scene.add_directional_light(bootstrap_light_entity) ||
        !bootstrap_scene.update_transforms()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_procedural_instance_resources() noexcept
{
    procedural_instances.resize(renderer::procedural::maximum_instance_count);

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    const VkDeviceSize atom_size = std::max<VkDeviceSize>(
        static_cast<VkDeviceSize>(properties.limits.nonCoherentAtomSize), 1U);
    const VkDeviceSize instance_data_size =
        static_cast<VkDeviceSize>(sizeof(renderer::procedural::InstanceData)) *
        renderer::procedural::maximum_instance_count;
    instance_slice_stride = ((instance_data_size + atom_size - 1U) / atom_size) * atom_size;
    instance_buffer_size = instance_slice_stride * frames_in_flight;
    instance_non_coherent_atom_size = atom_size;

    core::Status status = create_buffer_resource(instance_buffer_size,
                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                  instance_buffer,
                                                  instance_memory,
                                                  &instance_memory_properties);
    if (!status) {
        return status;
    }

    if (vkMapMemory(device,
                    instance_memory,
                    0,
                    instance_buffer_size,
                    0,
                    &instance_mapped) != VK_SUCCESS) {
        destroy_buffer_resource(instance_buffer, instance_memory);
        instance_buffer = VK_NULL_HANDLE;
        instance_memory = VK_NULL_HANDLE;
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    set_debug_name(VK_OBJECT_TYPE_BUFFER,
                   reinterpret_cast<std::uint64_t>(instance_buffer),
                   "GameEngine.ProceduralInstanceBuffer");

    status = set_procedural_workload(renderer::procedural::default_instance_count);
    if (!status) {
        destroy_procedural_instance_resources();
        return status;
    }
    return core::Status{};
}

void Renderer::Impl::destroy_procedural_instance_resources() noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    if (instance_mapped != nullptr && instance_memory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, instance_memory);
    }
    instance_mapped = nullptr;
    if (instance_buffer != VK_NULL_HANDLE || instance_memory != VK_NULL_HANDLE) {
        destroy_buffer_resource(instance_buffer, instance_memory);
    }
    instance_buffer = VK_NULL_HANDLE;
    instance_memory = VK_NULL_HANDLE;
    instance_slice_stride = 0;
    instance_buffer_size = 0;
    instance_non_coherent_atom_size = 1;
    instance_memory_properties = 0;
    procedural_instances.clear();
    active_instance_count = renderer::procedural::default_instance_count;
}

core::Status Renderer::Impl::set_procedural_workload(core::u32 instance_count) noexcept
{
    if (instance_count == 0U || instance_count > renderer::procedural::maximum_instance_count ||
        procedural_instances.size() < instance_count) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (!bootstrap_scene.update_transforms()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const scene::TransformComponent* cube_transform =
        bootstrap_scene.transform(bootstrap_cube_entity);
    if (cube_transform == nullptr ||
        !renderer::procedural::generate_instances(
            instance_count,
            cube_transform->world_matrix,
            std::span<renderer::procedural::ProceduralInstance>{procedural_instances.data(),
                                                                  instance_count})) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    active_instance_count = instance_count;
    return upload_gpu_source_instances();
}

core::Status Renderer::Impl::upload_gpu_source_instances() noexcept
{
    if (gpu_source_mapped == nullptr || gpu_source_memory == VK_NULL_HANDLE) {
        return core::Status{};
    }
    auto* destination = static_cast<renderer::gpu_culling::GpuCullInstance*>(gpu_source_mapped);
    for (core::u32 index = 0; index < active_instance_count; ++index) {
        destination[index] = renderer::gpu_culling::to_gpu_instance(procedural_instances[index]);
    }
    if ((gpu_source_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0U) {
        const VkDeviceSize written_size =
            static_cast<VkDeviceSize>(sizeof(renderer::gpu_culling::GpuCullInstance)) *
            active_instance_count;
        const VkDeviceSize atom_size = std::max<VkDeviceSize>(
            static_cast<VkDeviceSize>(gpu_source_buffer_size == 0U ? 1U :
                                                                       instance_non_coherent_atom_size),
            1U);
        const VkDeviceSize flush_size =
            ((written_size + atom_size - 1U) / atom_size) * atom_size;
        const VkMappedMemoryRange range{
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = gpu_source_memory,
            .offset = 0,
            .size = std::min(flush_size, gpu_source_buffer_size),
        };
        if (vkFlushMappedMemoryRanges(device, 1, &range) != VK_SUCCESS) {
            return core::Status{core::ErrorCode::vulkan_device_failed};
        }
    }
    return core::Status{};
}

core::Status Renderer::Impl::create_gpu_culling_resources() noexcept
{
    gpu_culling_available = false;
    gpu_culling_fallback = false;

    const QueueFamilies families = find_queue_families(physical_device);
    if (!families.graphics.has_value()) {
        return core::Status{};
    }
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> family_properties(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &family_count, family_properties.data());
    if (families.graphics.value() >= family_properties.size() ||
        (family_properties[families.graphics.value()].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0U) {
        core::log(core::LogLevel::warning,
                  "GPU culling unavailable: graphics queue has no compute support");
        return core::Status{};
    }

    const auto disable_gpu_culling = [this]() noexcept {
        destroy_gpu_culling_resources();
        core::log(core::LogLevel::warning,
                  "GPU culling resources unavailable; falling back to CPU culling");
    };

    const VkDeviceSize source_size =
        static_cast<VkDeviceSize>(sizeof(renderer::gpu_culling::GpuCullInstance)) *
        renderer::procedural::maximum_instance_count;
    if (!create_buffer_resource(source_size,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                gpu_source_buffer,
                                gpu_source_memory,
                                &gpu_source_memory_properties)
             .ok() ||
        vkMapMemory(device,
                    gpu_source_memory,
                    0,
                    source_size,
                    0,
                    &gpu_source_mapped) != VK_SUCCESS) {
        disable_gpu_culling();
        return core::Status{};
    }
    gpu_source_buffer_size = source_size;
    set_debug_name(VK_OBJECT_TYPE_BUFFER,
                   reinterpret_cast<std::uint64_t>(gpu_source_buffer),
                   "GameEngine.GpuCullSourceBuffer");

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    const VkDeviceSize storage_alignment = std::max<VkDeviceSize>(
        static_cast<VkDeviceSize>(properties.limits.minStorageBufferOffsetAlignment), 16U);
    const VkDeviceSize visible_data_size =
        static_cast<VkDeviceSize>(sizeof(renderer::procedural::InstanceData)) *
        renderer::procedural::maximum_instance_count;
    gpu_visible_slice_stride = ((visible_data_size + storage_alignment - 1U) /
                                storage_alignment) *
                               storage_alignment;
    gpu_visible_buffer_size = gpu_visible_slice_stride * frames_in_flight;
    if (!create_buffer_resource(gpu_visible_buffer_size,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                gpu_visible_buffer,
                                gpu_visible_memory)
             .ok()) {
        disable_gpu_culling();
        return core::Status{};
    }
    set_debug_name(VK_OBJECT_TYPE_BUFFER,
                   reinterpret_cast<std::uint64_t>(gpu_visible_buffer),
                   "GameEngine.GpuCullVisibleBuffer");

    const VkDeviceSize indirect_alignment = std::max<VkDeviceSize>(
        {static_cast<VkDeviceSize>(properties.limits.nonCoherentAtomSize),
         static_cast<VkDeviceSize>(properties.limits.minStorageBufferOffsetAlignment),
         4U});
    gpu_indirect_slice_stride =
        ((static_cast<VkDeviceSize>(sizeof(renderer::gpu_culling::IndirectCommand)) +
          indirect_alignment - 1U) /
         indirect_alignment) *
        indirect_alignment;
    gpu_indirect_buffer_size = gpu_indirect_slice_stride * frames_in_flight;
    if (!create_buffer_resource(gpu_indirect_buffer_size,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                gpu_indirect_buffer,
                                gpu_indirect_memory,
                                &gpu_indirect_memory_properties)
             .ok() ||
        vkMapMemory(device,
                    gpu_indirect_memory,
                    0,
                    gpu_indirect_buffer_size,
                    0,
                    &gpu_indirect_mapped) != VK_SUCCESS) {
        disable_gpu_culling();
        return core::Status{};
    }
    set_debug_name(VK_OBJECT_TYPE_BUFFER,
                   reinterpret_cast<std::uint64_t>(gpu_indirect_buffer),
                   "GameEngine.GpuCullIndirectBuffer");
    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        auto* command = reinterpret_cast<renderer::gpu_culling::IndirectCommand*>(
            static_cast<std::byte*>(gpu_indirect_mapped) +
            gpu_indirect_slice_stride * index);
        *command = renderer::gpu_culling::initial_indirect_command();
    }
    if ((gpu_indirect_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0U) {
        const VkMappedMemoryRange range{
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = gpu_indirect_memory,
            .offset = 0,
            .size = gpu_indirect_buffer_size,
        };
        if (vkFlushMappedMemoryRanges(device, 1, &range) != VK_SUCCESS) {
            disable_gpu_culling();
            return core::Status{};
        }
    }

    if (!create_gpu_culling_descriptors().ok() || !create_gpu_culling_pipeline().ok() ||
        !upload_gpu_source_instances().ok()) {
        disable_gpu_culling();
        return core::Status{};
    }
    gpu_culling_available = true;
    return core::Status{};
}

core::Status Renderer::Impl::create_gpu_culling_descriptors() noexcept
{
    const std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
        VkDescriptorSetLayoutBinding{0,
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     1,
                                     VK_SHADER_STAGE_COMPUTE_BIT,
                                     nullptr},
        VkDescriptorSetLayoutBinding{1,
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     1,
                                     VK_SHADER_STAGE_COMPUTE_BIT,
                                     nullptr},
        VkDescriptorSetLayoutBinding{2,
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     1,
                                     VK_SHADER_STAGE_COMPUTE_BIT,
                                     nullptr},
    };
    const VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    if (vkCreateDescriptorSetLayout(device,
                                    &layout_info,
                                    nullptr,
                                    &gpu_cull_descriptor_set_layout) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    const VkDescriptorPoolSize pool_size{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        static_cast<std::uint32_t>(bindings.size()) * frames_in_flight,
    };
    const VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = frames_in_flight,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    if (vkCreateDescriptorPool(device,
                               &pool_info,
                               nullptr,
                               &gpu_cull_descriptor_pool) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    const std::array<VkDescriptorSetLayout, frames_in_flight> layouts = {
        gpu_cull_descriptor_set_layout,
        gpu_cull_descriptor_set_layout,
    };
    const VkDescriptorSetAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = gpu_cull_descriptor_pool,
        .descriptorSetCount = frames_in_flight,
        .pSetLayouts = layouts.data(),
    };
    if (vkAllocateDescriptorSets(device, &allocate_info, gpu_cull_descriptor_sets.data()) !=
        VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    for (core::u32 index = 0; index < frames_in_flight; ++index) {
        const std::array<VkDescriptorBufferInfo, 3> buffer_infos = {
            VkDescriptorBufferInfo{gpu_source_buffer, 0, gpu_source_buffer_size},
            VkDescriptorBufferInfo{gpu_visible_buffer,
                                   gpu_visible_slice_stride * index,
                                   static_cast<VkDeviceSize>(sizeof(renderer::procedural::InstanceData)) *
                                       renderer::procedural::maximum_instance_count},
            VkDescriptorBufferInfo{gpu_indirect_buffer,
                                   gpu_indirect_slice_stride * index,
                                   sizeof(renderer::gpu_culling::IndirectCommand)},
        };
        std::array<VkWriteDescriptorSet, 3> writes{};
        for (core::u32 binding = 0; binding < writes.size(); ++binding) {
            writes[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[binding].dstSet = gpu_cull_descriptor_sets[index];
            writes[binding].dstBinding = binding;
            writes[binding].descriptorCount = 1;
            writes[binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[binding].pBufferInfo = &buffer_infos[binding];
        }
        vkUpdateDescriptorSets(device,
                               static_cast<std::uint32_t>(writes.size()),
                               writes.data(),
                               0,
                               nullptr);
    }
    set_debug_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                   reinterpret_cast<std::uint64_t>(gpu_cull_descriptor_set_layout),
                   "GameEngine.GpuCullSetLayout");
    set_debug_name(VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                   reinterpret_cast<std::uint64_t>(gpu_cull_descriptor_pool),
                   "GameEngine.GpuCullPool");
    return core::Status{};
}

core::Status Renderer::Impl::create_gpu_culling_pipeline() noexcept
{
    if (compute_shader_artifact == nullptr) {
        return core::Status{core::ErrorCode::shader_variant_unavailable};
    }
    const VkPushConstantRange push_constant_range{
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(renderer::gpu_culling::GpuCullPushConstants),
    };
    const VkPipelineLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &gpu_cull_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    if (vkCreatePipelineLayout(device,
                               &layout_info,
                               nullptr,
                               &gpu_cull_pipeline_layout) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    const VkShaderModule shader_module = create_shader_module(
        compute_shader_artifact->spirv,
        compute_shader_artifact->spirv_word_count * sizeof(std::uint32_t));
    if (shader_module == VK_NULL_HANDLE) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    const VkPipelineShaderStageCreateInfo stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName = compute_shader_artifact->entry_point.data(),
    };
    const VkComputePipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage_info,
        .layout = gpu_cull_pipeline_layout,
    };
    const VkResult result = vkCreateComputePipelines(device,
                                                     pipeline_cache,
                                                     1,
                                                     &pipeline_info,
                                                     nullptr,
                                                     &gpu_cull_pipeline);
    vkDestroyShaderModule(device, shader_module, nullptr);
    if (result != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    set_debug_name(VK_OBJECT_TYPE_PIPELINE,
                   reinterpret_cast<std::uint64_t>(gpu_cull_pipeline),
                   "GameEngine.GpuCullPipeline");
    return core::Status{};
}

void Renderer::Impl::destroy_gpu_culling_pipeline() noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    if (gpu_cull_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, gpu_cull_pipeline, nullptr);
        gpu_cull_pipeline = VK_NULL_HANDLE;
    }
    if (gpu_cull_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, gpu_cull_pipeline_layout, nullptr);
        gpu_cull_pipeline_layout = VK_NULL_HANDLE;
    }
    if (gpu_cull_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, gpu_cull_descriptor_pool, nullptr);
        gpu_cull_descriptor_pool = VK_NULL_HANDLE;
    }
    if (gpu_cull_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, gpu_cull_descriptor_set_layout, nullptr);
        gpu_cull_descriptor_set_layout = VK_NULL_HANDLE;
    }
    gpu_cull_descriptor_sets = {};
}

void Renderer::Impl::destroy_gpu_culling_resources() noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    destroy_gpu_culling_pipeline();
    if (gpu_source_mapped != nullptr && gpu_source_memory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, gpu_source_memory);
    }
    gpu_source_mapped = nullptr;
    if (gpu_source_buffer != VK_NULL_HANDLE || gpu_source_memory != VK_NULL_HANDLE) {
        destroy_buffer_resource(gpu_source_buffer, gpu_source_memory);
    }
    if (gpu_visible_buffer != VK_NULL_HANDLE || gpu_visible_memory != VK_NULL_HANDLE) {
        destroy_buffer_resource(gpu_visible_buffer, gpu_visible_memory);
    }
    if (gpu_indirect_mapped != nullptr && gpu_indirect_memory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, gpu_indirect_memory);
    }
    gpu_indirect_mapped = nullptr;
    if (gpu_indirect_buffer != VK_NULL_HANDLE || gpu_indirect_memory != VK_NULL_HANDLE) {
        destroy_buffer_resource(gpu_indirect_buffer, gpu_indirect_memory);
    }
    gpu_source_buffer = VK_NULL_HANDLE;
    gpu_source_memory = VK_NULL_HANDLE;
    gpu_source_buffer_size = 0;
    gpu_source_memory_properties = 0;
    gpu_visible_buffer = VK_NULL_HANDLE;
    gpu_visible_memory = VK_NULL_HANDLE;
    gpu_visible_slice_stride = 0;
    gpu_visible_buffer_size = 0;
    gpu_indirect_buffer = VK_NULL_HANDLE;
    gpu_indirect_memory = VK_NULL_HANDLE;
    gpu_indirect_slice_stride = 0;
    gpu_indirect_buffer_size = 0;
    gpu_indirect_memory_properties = 0;
    gpu_culling_available = false;
}

core::Status Renderer::Impl::set_visibility_mode(
    renderer::gpu_culling::VisibilityMode mode) noexcept
{
    gpu_culling_fallback = mode == renderer::gpu_culling::VisibilityMode::gpu &&
                           !gpu_culling_available;
    visibility_mode = renderer::gpu_culling::resolve_visibility_mode(mode,
                                                                       gpu_culling_available);
    const core::Status graph_status = create_render_graph();
    timing_accumulator.reset();
    return graph_status;
}

core::Status Renderer::Impl::create_bootstrap_material_resources() noexcept
{
    const rhi::ImageDescription image_description{
        .width = scene::bootstrap_texture_width,
        .height = scene::bootstrap_texture_height,
        .format = rhi::ImageFormat::rgba8_unorm,
    };
    core::Status status = create_image(image_description, bootstrap_albedo_image);
    if (!status) {
        return status;
    }
    status = upload_image(bootstrap_albedo_image, scene::bootstrap_checkerboard);
    if (!status) {
        return status;
    }
    status = create_sampler(
        {.min_filter = rhi::SamplerFilter::linear, .mag_filter = rhi::SamplerFilter::linear},
        bootstrap_albedo_sampler);
    if (!status) {
        return status;
    }

    status = create_buffer_resource(sizeof(scene::BootstrapMaterialConstants),
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    material_uniform_buffer,
                                    material_uniform_memory);
    if (!status) {
        return status;
    }

    const scene::BootstrapMaterialConstants material_constants{};
    void* mapped = nullptr;
    if (vkMapMemory(device,
                    material_uniform_memory,
                    0,
                    sizeof(material_constants),
                    0,
                    &mapped) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    std::memcpy(mapped, &material_constants, sizeof(material_constants));
    vkUnmapMemory(device, material_uniform_memory);
    set_debug_name(VK_OBJECT_TYPE_BUFFER,
                   reinterpret_cast<std::uint64_t>(material_uniform_buffer),
                   "GameEngine.BootstrapMaterialUniformBuffer");
    return core::Status{};
}

core::Status Renderer::Impl::create_material_descriptors() noexcept
{
    ImageSlot* image = nullptr;
    SamplerSlot* sampler = nullptr;
    if (!validate_image(bootstrap_albedo_image, image) ||
        !validate_sampler(bootstrap_albedo_sampler, sampler) ||
        material_uniform_buffer == VK_NULL_HANDLE) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    const std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
        VkDescriptorSetLayoutBinding{0,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     1,
                                     VK_SHADER_STAGE_FRAGMENT_BIT,
                                     nullptr},
        VkDescriptorSetLayoutBinding{1,
                                     VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                     1,
                                     VK_SHADER_STAGE_FRAGMENT_BIT,
                                     nullptr},
        VkDescriptorSetLayoutBinding{2,
                                     VK_DESCRIPTOR_TYPE_SAMPLER,
                                     1,
                                     VK_SHADER_STAGE_FRAGMENT_BIT,
                                     nullptr},
    };
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(device,
                                    &layout_info,
                                    nullptr,
                                    &material_descriptor_set_layout) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    const std::array<VkDescriptorPoolSize, 3> pool_sizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1},
    };
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    if (vkCreateDescriptorPool(device,
                                &pool_info,
                                nullptr,
                                &material_descriptor_pool) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = material_descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &material_descriptor_set_layout;
    if (vkAllocateDescriptorSets(device, &allocate_info, &material_descriptor_set) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = material_uniform_buffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(scene::BootstrapMaterialConstants);
    VkDescriptorImageInfo image_info{};
    image_info.imageView = image->view;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo sampler_info{};
    sampler_info.sampler = sampler->sampler;
    const std::array<VkWriteDescriptorSet, 3> writes = {
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             nullptr,
                             material_descriptor_set,
                             0,
                             0,
                             1,
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             nullptr,
                             &buffer_info,
                             nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             nullptr,
                             material_descriptor_set,
                             1,
                             0,
                             1,
                             VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                             &image_info,
                             nullptr,
                             nullptr},
        VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             nullptr,
                             material_descriptor_set,
                             2,
                             0,
                             1,
                             VK_DESCRIPTOR_TYPE_SAMPLER,
                             &sampler_info,
                             nullptr,
                             nullptr},
    };
    vkUpdateDescriptorSets(device,
                           static_cast<std::uint32_t>(writes.size()),
                           writes.data(),
                           0,
                           nullptr);
    set_debug_name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                   reinterpret_cast<std::uint64_t>(material_descriptor_set_layout),
                   "GameEngine.BootstrapMaterialSetLayout");
    set_debug_name(VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                   reinterpret_cast<std::uint64_t>(material_descriptor_pool),
                   "GameEngine.BootstrapMaterialPool");
    return core::Status{};
}

void Renderer::Impl::destroy_material_resources() noexcept
{
    if (device == VK_NULL_HANDLE) {
        return;
    }
    material_descriptor_set = VK_NULL_HANDLE;
    if (material_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, material_descriptor_pool, nullptr);
        material_descriptor_pool = VK_NULL_HANDLE;
    }
    if (material_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, material_descriptor_set_layout, nullptr);
        material_descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (material_uniform_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, material_uniform_buffer, nullptr);
        material_uniform_buffer = VK_NULL_HANDLE;
    }
    if (material_uniform_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, material_uniform_memory, nullptr);
        material_uniform_memory = VK_NULL_HANDLE;
    }
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
                                                    VkDeviceMemory& memory,
                                                    VkMemoryPropertyFlags* allocated_properties) noexcept
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

    if (allocated_properties != nullptr) {
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
        *allocated_properties = memory_properties.memoryTypes[memory_type].propertyFlags;
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
        (description.usage != rhi::BufferUsage::vertex &&
         description.usage != rhi::BufferUsage::index)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }

    const VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        (description.usage == rhi::BufferUsage::vertex ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                                         : VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    core::Status status = allocate_buffer_slot(handle);
    if (!status) {
        return status;
    }
    BufferSlot& slot = buffers[handle.index];
    status = create_buffer_resource(static_cast<VkDeviceSize>(description.size),
                                    usage,
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
                   description.usage == rhi::BufferUsage::vertex ? "GameEngine.VertexBuffer"
                                                                  : "GameEngine.IndexBuffer");
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
    if (description.vertex_layout != rhi::PipelineVertexLayout::position3_color3 ||
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

core::Status Renderer::Impl::reload_shaders() noexcept
{
    if (!shader_hot_reload_enabled) {
        return core::Status{core::ErrorCode::shader_reload_disabled};
    }
    if (device == VK_NULL_HANDLE || vkDeviceWaitIdle(device) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }

    const auto previous_vertex_shader_artifact = vertex_shader_artifact;
    const auto previous_fragment_shader_artifact = fragment_shader_artifact;
    const core::Status variant_status = select_shader_variants();
    if (!variant_status) {
        return variant_status;
    }

    std::vector<VkPipeline> replacements(pipelines.size(), VK_NULL_HANDLE);
    for (std::size_t index = 0; index < pipelines.size(); ++index) {
        PipelineSlot& slot = pipelines[index];
        if (slot.state != ResourceState::live) {
            continue;
        }
        const core::Status status = build_pipeline_object(slot, replacements[index]);
        if (!status) {
            for (VkPipeline replacement : replacements) {
                if (replacement != VK_NULL_HANDLE) {
                    vkDestroyPipeline(device, replacement, nullptr);
                }
            }
            vertex_shader_artifact = previous_vertex_shader_artifact;
            fragment_shader_artifact = previous_fragment_shader_artifact;
            return status;
        }
    }

    for (std::size_t index = 0; index < pipelines.size(); ++index) {
        PipelineSlot& slot = pipelines[index];
        if (slot.state == ResourceState::live && replacements[index] != VK_NULL_HANDLE) {
            destroy_pipeline_object(slot);
            slot.pipeline = replacements[index];
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
    if (handle.index == cube_pipeline.index && handle.generation == cube_pipeline.generation) {
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
    destroy_render_finished_semaphores();
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
    if (depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depth_image_view, nullptr);
        depth_image_view = VK_NULL_HANDLE;
    }
    if (depth_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, depth_image, nullptr);
        depth_image = VK_NULL_HANDLE;
    }
    if (depth_memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, depth_memory, nullptr);
        depth_memory = VK_NULL_HANDLE;
    }
    depth_format = VK_FORMAT_UNDEFINED;
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
                                                core::u32 image_index,
                                                core::u32 frame_index) noexcept
{
    if (image_index >= framebuffers.size() || frame_index >= frames_in_flight ||
        !render_graph.compiled() || render_graph.execution_order().empty()) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const PipelineSlot* pipeline = nullptr;
    if (cube_pipeline.valid() && cube_pipeline.index < pipelines.size()) {
        const PipelineSlot& candidate = pipelines[cube_pipeline.index];
        if (candidate.state == ResourceState::live &&
            candidate.generation == cube_pipeline.generation) {
            pipeline = &candidate;
        }
    }
    if (pipeline == nullptr || pipeline->pipeline == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const BufferSlot* vertex_buffer = nullptr;
    if (cube_vertex_buffer.valid() && cube_vertex_buffer.index < buffers.size()) {
        const BufferSlot& candidate = buffers[cube_vertex_buffer.index];
        if (candidate.state == ResourceState::live &&
            candidate.generation == cube_vertex_buffer.generation) {
            vertex_buffer = &candidate;
        }
    }
    const BufferSlot* index_buffer = nullptr;
    if (cube_index_buffer.valid() && cube_index_buffer.index < buffers.size()) {
        const BufferSlot& candidate = buffers[cube_index_buffer.index];
        if (candidate.state == ResourceState::live &&
            candidate.generation == cube_index_buffer.generation) {
            index_buffer = &candidate;
        }
    }
    if (vertex_buffer == nullptr || vertex_buffer->buffer == VK_NULL_HANDLE ||
        index_buffer == nullptr || index_buffer->buffer == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!bootstrap_scene.update_transforms()) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const scene::TransformComponent* camera_transform =
        bootstrap_scene.transform(bootstrap_camera_entity);
    const scene::CameraComponent* camera_component =
        bootstrap_scene.camera(bootstrap_camera_entity);
    const scene::DirectionalLightComponent* light_component =
        bootstrap_scene.directional_light(bootstrap_light_entity);
    if (camera_transform == nullptr || camera_component == nullptr || light_component == nullptr) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const math::Vec3 camera_position{
        camera_transform->world_matrix.at(0, 3),
        camera_transform->world_matrix.at(1, 3),
        camera_transform->world_matrix.at(2, 3),
    };
    scene::BootstrapMaterialConstants material_constants{};
    material_constants.light_direction_intensity = {
        light_component->direction.x,
        light_component->direction.y,
        light_component->direction.z,
        light_component->intensity,
    };
    material_constants.camera_position_exposure = {
        camera_position.x,
        camera_position.y,
        camera_position.z,
        1.0F,
    };
    void* material_mapped = nullptr;
    if (vkMapMemory(device,
                    material_uniform_memory,
                    0,
                    sizeof(material_constants),
                    0,
                    &material_mapped) != VK_SUCCESS) {
        return VK_ERROR_DEVICE_LOST;
    }
    std::memcpy(material_mapped, &material_constants, sizeof(material_constants));
    vkUnmapMemory(device, material_uniform_memory);

    const core::f32 aspect_ratio = static_cast<core::f32>(swapchain_extent.width) /
                                    static_cast<core::f32>(swapchain_extent.height);
    const math::Mat4 view = math::look_at_rh(
        camera_position, {0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
    const math::Mat4 projection = math::perspective_rh_zo(
        camera_component->vertical_field_of_view_radians,
        aspect_ratio,
        camera_component->near_plane,
        camera_component->far_plane);
    const math::Mat4 view_projection = math::multiply(projection, view);

    const bool use_gpu_culling = visibility_mode == renderer::gpu_culling::VisibilityMode::gpu &&
                                 gpu_culling_available;
    auto& timing = frame_timing[frame_index];
    timing.reset(gpu_timestamps_enabled,
                 visibility_mode,
                 gpu_culling_available,
                 use_gpu_culling,
                 gpu_culling_fallback);
    timing.total_instances = active_instance_count;
    timing.instance_buffer_bytes = instance_buffer_size;
    timing.gpu_source_buffer_bytes = gpu_source_buffer_size;
    timing.gpu_visible_buffer_bytes = gpu_visible_buffer_size;
    timing.gpu_indirect_buffer_bytes = gpu_indirect_buffer_size;

    const auto visibility_start = std::chrono::steady_clock::now();
    math::Frustum frustum{};
    static_cast<void>(math::extract_frustum_rh_zo(view_projection, frustum));
    const VkDeviceSize instance_offset = instance_slice_stride * frame_index;
    core::u32 visible_count = 0U;
    if (use_gpu_culling) {
        if (gpu_indirect_mapped == nullptr || gpu_indirect_buffer == VK_NULL_HANDLE ||
            gpu_visible_buffer == VK_NULL_HANDLE || gpu_cull_pipeline == VK_NULL_HANDLE) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        auto* command = reinterpret_cast<renderer::gpu_culling::IndirectCommand*>(
            static_cast<std::byte*>(gpu_indirect_mapped) +
            gpu_indirect_slice_stride * frame_index);
        *command = renderer::gpu_culling::initial_indirect_command();
        if ((gpu_indirect_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0U) {
            const VkMappedMemoryRange range{
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = gpu_indirect_memory,
                .offset = gpu_indirect_slice_stride * frame_index,
                .size = gpu_indirect_slice_stride,
            };
            if (vkFlushMappedMemoryRanges(device, 1, &range) != VK_SUCCESS) {
                return VK_ERROR_DEVICE_LOST;
            }
        }
        timing.culled_instances = active_instance_count;
    } else {
        if (instance_mapped == nullptr || instance_buffer == VK_NULL_HANDLE ||
            instance_offset + sizeof(renderer::procedural::InstanceData) * active_instance_count >
                instance_buffer_size) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        auto* visible_data = reinterpret_cast<renderer::procedural::InstanceData*>(
            static_cast<std::byte*>(instance_mapped) + instance_offset);
        if (!renderer::procedural::cull_instances(
                std::span<const renderer::procedural::ProceduralInstance>{
                    procedural_instances.data(), active_instance_count},
                frustum,
                std::span<renderer::procedural::InstanceData>{visible_data, active_instance_count},
                visible_count)) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        timing.visible_instances = visible_count;
        timing.culled_instances = active_instance_count - visible_count;
    }
    const auto visibility_end = std::chrono::steady_clock::now();
    const core::u64 preparation_nanoseconds = static_cast<core::u64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(visibility_end - visibility_start)
            .count());
    timing.visibility_cpu_nanoseconds = use_gpu_culling ? 0U : preparation_nanoseconds;
    timing.gpu_culling_cpu_nanoseconds = use_gpu_culling ? preparation_nanoseconds : 0U;
    if (!use_gpu_culling &&
        (instance_memory_properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0U &&
        visible_count > 0U) {
        const VkDeviceSize written_size =
            static_cast<VkDeviceSize>(sizeof(renderer::procedural::InstanceData)) * visible_count;
        const VkDeviceSize flush_size = std::min(
            instance_slice_stride,
            ((written_size + instance_non_coherent_atom_size - 1U) /
             instance_non_coherent_atom_size) *
                instance_non_coherent_atom_size);
        const VkMappedMemoryRange range{
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = instance_memory,
            .offset = instance_offset,
            .size = flush_size,
        };
        if (vkFlushMappedMemoryRanges(device, 1, &range) != VK_SUCCESS) {
            return VK_ERROR_DEVICE_LOST;
        }
    }

    const auto execution_order = render_graph.execution_order();
    if (execution_order.size() > renderer::metrics::max_timed_passes) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return result;
    }

    for (const renderer::render_graph::PassHandle pass : execution_order) {
        const std::string_view pass_name = render_graph.pass_name(pass);
        if (use_gpu_culling && pass.index == gpu_cull_pass.index && pass_name == "gpu_cull") {
            const auto pass_start = std::chrono::steady_clock::now();
            const core::u32 query_base =
                frame_index * timestamp_queries_per_frame + timing.pass_count * 2U;
            if (gpu_timestamps_enabled) {
                vkCmdWriteTimestamp(command_buffer,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    timestamp_query_pool,
                                    query_base);
            }
            const renderer::gpu_culling::GpuCullPushConstants cull_constants =
                renderer::gpu_culling::make_push_constants(frustum, active_instance_count);
            begin_debug_label(command_buffer, "GameEngine.GpuCull");
            vkCmdBindPipeline(command_buffer,
                              VK_PIPELINE_BIND_POINT_COMPUTE,
                              gpu_cull_pipeline);
            vkCmdBindDescriptorSets(command_buffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    gpu_cull_pipeline_layout,
                                    0,
                                    1,
                                    &gpu_cull_descriptor_sets[frame_index],
                                    0,
                                    nullptr);
            vkCmdPushConstants(command_buffer,
                               gpu_cull_pipeline_layout,
                               VK_SHADER_STAGE_COMPUTE_BIT,
                               0,
                               sizeof(cull_constants),
                               &cull_constants);
            vkCmdDispatch(command_buffer,
                          renderer::gpu_culling::dispatch_group_count(active_instance_count),
                          1,
                          1);
            const std::array<VkBufferMemoryBarrier, 2> buffer_barriers = {
                VkBufferMemoryBarrier{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = gpu_visible_buffer,
                    .offset = gpu_visible_slice_stride * frame_index,
                    .size = static_cast<VkDeviceSize>(sizeof(renderer::procedural::InstanceData)) *
                            renderer::procedural::maximum_instance_count,
                },
                VkBufferMemoryBarrier{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = gpu_indirect_buffer,
                    .offset = gpu_indirect_slice_stride * frame_index,
                    .size = sizeof(renderer::gpu_culling::IndirectCommand),
                },
            };
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                                     VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 static_cast<std::uint32_t>(buffer_barriers.size()),
                                 buffer_barriers.data(),
                                 0,
                                 nullptr);
            end_debug_label(command_buffer);
            if (gpu_timestamps_enabled) {
                vkCmdWriteTimestamp(command_buffer,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    timestamp_query_pool,
                                    query_base + 1U);
            }
            const auto pass_end = std::chrono::steady_clock::now();
            timing.add_pass(
                pass_name,
                static_cast<core::u64>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(pass_end - pass_start)
                        .count()),
                0U,
                gpu_timestamps_enabled);
            continue;
        }
        if (pass.index != forward_opaque_pass.index || pass_name != "forward_opaque") {
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        const auto pass_start = std::chrono::steady_clock::now();
        const core::u32 query_base =
            frame_index * timestamp_queries_per_frame + timing.pass_count * 2U;
        if (gpu_timestamps_enabled) {
            vkCmdWriteTimestamp(command_buffer,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                timestamp_query_pool,
                                query_base);
        }

        const std::array<VkClearValue, 2> clear_values = {
            VkClearValue{.color = {{0.02F, 0.03F, 0.06F, 1.0F}}},
            VkClearValue{.depthStencil = {1.0F, 0}},
        };
        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = framebuffers[image_index];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = swapchain_extent;
        render_pass_info.clearValueCount = static_cast<std::uint32_t>(clear_values.size());
        render_pass_info.pClearValues = clear_values.data();

        vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchain_extent.width);
        viewport.height = static_cast<float>(swapchain_extent.height);
        viewport.maxDepth = 1.0F;
        VkRect2D scissor{};
        scissor.extent = swapchain_extent;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
        const std::array<VkBuffer, 2> vertex_buffers = {
            vertex_buffer->buffer,
            use_gpu_culling ? gpu_visible_buffer : instance_buffer,
        };
        const std::array<VkDeviceSize, 2> vertex_offsets = {
            0,
            use_gpu_culling ? gpu_visible_slice_stride * frame_index : instance_offset,
        };
        vkCmdBindVertexBuffers(command_buffer,
                               0,
                               static_cast<std::uint32_t>(vertex_buffers.size()),
                               vertex_buffers.data(),
                               vertex_offsets.data());
        vkCmdBindIndexBuffer(command_buffer, index_buffer->buffer, 0, VK_INDEX_TYPE_UINT16);
        const ViewProjectionPushConstants push_constants{.view_projection = view_projection};
        begin_debug_label(command_buffer, "GameEngine.ForwardOpaque");
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindDescriptorSets(command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout,
                                0,
                                1,
                                &material_descriptor_set,
                                0,
                                nullptr);
        vkCmdPushConstants(command_buffer,
                           pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(push_constants),
                           &push_constants);
        if (use_gpu_culling) {
            vkCmdDrawIndexedIndirect(command_buffer,
                                     gpu_indirect_buffer,
                                     gpu_indirect_slice_stride * frame_index,
                                     1,
                                     sizeof(renderer::gpu_culling::IndirectCommand));
        } else if (visible_count > 0U) {
            vkCmdDrawIndexed(command_buffer,
                             static_cast<std::uint32_t>(scene::bootstrap_cube_indices.size()),
                             visible_count,
                             0,
                             0,
                             0);
        }
        end_debug_label(command_buffer);
        vkCmdEndRenderPass(command_buffer);
        if (gpu_timestamps_enabled) {
            vkCmdWriteTimestamp(command_buffer,
                                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                timestamp_query_pool,
                                query_base + 1U);
        }

        const auto pass_end = std::chrono::steady_clock::now();
        const auto cpu_nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                         pass_end - pass_start)
                                         .count();
        frame_timing[frame_index].add_pass(
            pass_name,
            static_cast<core::u64>(cpu_nanoseconds),
            use_gpu_culling || visible_count > 0U ? 1U : 0U,
            gpu_timestamps_enabled);
    }

    result = vkEndCommandBuffer(command_buffer);
    if (result == VK_SUCCESS) {
        timing_pending[frame_index] = true;
    }
    return result;
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
    resolve_gpu_visibility(current_frame);
    resolve_timing(current_frame);
    reset_timing_queries(current_frame);
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
    if (image_index >= images_in_flight.size() || image_index >= render_finished.size()) {
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
        record_command_buffer(command_buffers[image_index], image_index, current_frame) !=
            VK_SUCCESS) {
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
    submit_info.pSignalSemaphores = &render_finished[image_index];
    if (vkQueueSubmit(graphics_queue,
                      1,
                      &submit_info,
                      in_flight_fences[current_frame]) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_frame_failed};
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished[image_index];
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

core::Status Renderer::initialize(const platform::Platform& platform,
                                  const RendererConfiguration& configuration) noexcept
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
                                                  platform.window_size(),
                                                  configuration);
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

core::Status Renderer::reload_shaders() noexcept
{
    if (!initialized_ || impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->reload_shaders();
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

namespace gameengine::renderer::diagnostics {

void begin_metrics(const gameengine::rhi::Renderer& renderer) noexcept
{
    if (renderer.impl_ == nullptr) {
        return;
    }
    if (renderer.impl_->device != VK_NULL_HANDLE &&
        vkDeviceWaitIdle(renderer.impl_->device) != VK_SUCCESS) {
        return;
    }
    renderer.impl_->resolve_all_timing();
    renderer.impl_->timing_accumulator.reset();
}

core::Status set_visibility_mode(const gameengine::rhi::Renderer& renderer,
                                 gpu_culling::VisibilityMode mode) noexcept
{
    if (renderer.impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (renderer.impl_->device == VK_NULL_HANDLE ||
        vkDeviceWaitIdle(renderer.impl_->device) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    renderer.impl_->resolve_all_timing();
    return renderer.impl_->set_visibility_mode(mode);
}

core::Status set_procedural_workload(const gameengine::rhi::Renderer& renderer,
                                     core::u32 instance_count) noexcept
{
    if (renderer.impl_ == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (renderer.impl_->device == VK_NULL_HANDLE ||
        vkDeviceWaitIdle(renderer.impl_->device) != VK_SUCCESS) {
        return core::Status{core::ErrorCode::vulkan_device_failed};
    }
    renderer.impl_->resolve_all_timing();
    const core::Status status = renderer.impl_->set_procedural_workload(instance_count);
    if (status) {
        renderer.impl_->timing_accumulator.reset();
    }
    return status;
}

void print_metrics(const gameengine::rhi::Renderer& renderer) noexcept
{
    if (renderer.impl_ == nullptr) {
        std::fprintf(stderr, "[gameengine] [info] renderer metrics unavailable\n");
        return;
    }
    if (renderer.impl_->device != VK_NULL_HANDLE &&
        vkDeviceWaitIdle(renderer.impl_->device) != VK_SUCCESS) {
        std::fprintf(stderr, "[gameengine] [warning] renderer metrics synchronization failed\n");
        return;
    }
    renderer.impl_->resolve_all_timing();
    const auto& accumulator = renderer.impl_->timing_accumulator;
    const char* mode = accumulator.visibility_mode == gpu_culling::VisibilityMode::gpu
                           ? "gpu"
                           : "cpu";
    const char* gpu_state = accumulator.gpu_culling_fallback
                                ? "fallback"
                                : !accumulator.gpu_culling_available
                                      ? "unavailable"
                                      : accumulator.gpu_culling_active ? "active" : "available";
    std::fprintf(stderr,
                 "[gameengine] [info] renderer metrics: mode=%s gpu_culling=%s instances=%u frames=%llu "
                 "draw_calls=%llu visible_avg=%llu culled_avg=%llu "
                 "instance_buffer_bytes=%llu gpu_source_bytes=%llu gpu_visible_bytes=%llu "
                 "gpu_indirect_bytes=%llu gpu_timestamps=%s\n",
                 mode,
                 gpu_state,
                 renderer.impl_->active_instance_count,
                 static_cast<unsigned long long>(accumulator.frame_count),
                 static_cast<unsigned long long>(accumulator.total_draw_calls),
                 static_cast<unsigned long long>(accumulator.frame_count == 0U
                                                     ? 0U
                                                     : accumulator.visible_instances /
                                                           accumulator.frame_count),
                 static_cast<unsigned long long>(accumulator.frame_count == 0U
                                                     ? 0U
                                                     : accumulator.culled_instances /
                                                           accumulator.frame_count),
                 static_cast<unsigned long long>(accumulator.instance_buffer_bytes),
                 static_cast<unsigned long long>(accumulator.gpu_source_buffer_bytes),
                 static_cast<unsigned long long>(accumulator.gpu_visible_buffer_bytes),
                 static_cast<unsigned long long>(accumulator.gpu_indirect_buffer_bytes),
                 accumulator.gpu_timestamps_available ? "available" : "unavailable");
    if (accumulator.frame_count > 0U) {
        if (accumulator.visibility_mode == gpu_culling::VisibilityMode::gpu) {
            std::fprintf(stderr,
                         "[gameengine] [info] gpu_culling_cpu_ns(avg/min/max)=%llu/%llu/%llu\n",
                         static_cast<unsigned long long>(
                             accumulator.gpu_culling_cpu_total_nanoseconds /
                             accumulator.frame_count),
                         static_cast<unsigned long long>(
                             accumulator.gpu_culling_cpu_min_nanoseconds),
                         static_cast<unsigned long long>(
                             accumulator.gpu_culling_cpu_max_nanoseconds));
        } else {
            std::fprintf(stderr,
                         "[gameengine] [info] visibility_cpu_ns(avg/min/max)=%llu/%llu/%llu\n",
                         static_cast<unsigned long long>(accumulator.visibility_cpu_total_nanoseconds /
                                                         accumulator.frame_count),
                         static_cast<unsigned long long>(accumulator.visibility_cpu_min_nanoseconds),
                         static_cast<unsigned long long>(accumulator.visibility_cpu_max_nanoseconds));
        }
    }
    for (const auto& pass : accumulator.passes) {
        if (pass.name.empty() || pass.sample_count == 0U) {
            continue;
        }
        const auto cpu_average = pass.cpu_total_nanoseconds / pass.sample_count;
        std::fprintf(stderr,
                     "[gameengine] [info] pass=%.*s cpu_ns(avg/min/max)=%llu/%llu/%llu "
                     "draw_calls_avg=%llu",
                     static_cast<int>(pass.name.size()),
                     pass.name.data(),
                     static_cast<unsigned long long>(cpu_average),
                     static_cast<unsigned long long>(pass.cpu_min_nanoseconds),
                     static_cast<unsigned long long>(pass.cpu_max_nanoseconds),
                     static_cast<unsigned long long>(pass.draw_calls / pass.sample_count));
        if (pass.gpu_sample_count > 0U) {
            std::fprintf(stderr,
                         " gpu_ns(avg/min/max)=%llu/%llu/%llu",
                         static_cast<unsigned long long>(pass.gpu_total_nanoseconds /
                                                         pass.gpu_sample_count),
                         static_cast<unsigned long long>(pass.gpu_min_nanoseconds),
                         static_cast<unsigned long long>(pass.gpu_max_nanoseconds));
        }
        std::fputc('\n', stderr);
    }
}

} // namespace gameengine::renderer::diagnostics
