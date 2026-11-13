#pragma once

#include <span>

#include "engine/core/status.hpp"
#include "engine/platform/platform.hpp"
#include "engine/rhi/rhi.hpp"

namespace gameengine::rhi {

class RendererBackend {
public:
    virtual ~RendererBackend() noexcept = default;

    [[nodiscard]] virtual core::Status initialize(
        platform::NativeWindowHandles handles,
        platform::WindowSize size,
        const RendererConfiguration& configuration) noexcept = 0;
    [[nodiscard]] virtual core::Status render_frame(platform::WindowSize size) noexcept = 0;
    [[nodiscard]] virtual core::Status reload_shaders() noexcept = 0;

    [[nodiscard]] virtual core::Status create_buffer(const BufferDescription& description,
                                                     BufferHandle& handle) noexcept = 0;
    [[nodiscard]] virtual core::Status upload_buffer(BufferHandle handle,
                                                     std::span<const std::byte> data) noexcept = 0;
    [[nodiscard]] virtual core::Status destroy_buffer(BufferHandle handle) noexcept = 0;

    [[nodiscard]] virtual core::Status create_image(const ImageDescription& description,
                                                    ImageHandle& handle) noexcept = 0;
    [[nodiscard]] virtual core::Status upload_image(ImageHandle handle,
                                                    std::span<const std::byte> data) noexcept = 0;
    [[nodiscard]] virtual core::Status destroy_image(ImageHandle handle) noexcept = 0;

    [[nodiscard]] virtual core::Status create_sampler(const SamplerDescription& description,
                                                      SamplerHandle& handle) noexcept = 0;
    [[nodiscard]] virtual core::Status destroy_sampler(SamplerHandle handle) noexcept = 0;

    [[nodiscard]] virtual core::Status create_graphics_pipeline(
        const GraphicsPipelineDescription& description,
        PipelineHandle& handle) noexcept = 0;
    [[nodiscard]] virtual core::Status destroy_pipeline(PipelineHandle handle) noexcept = 0;

    virtual void shutdown() noexcept = 0;

    [[nodiscard]] virtual BackendCapabilities capabilities() const noexcept = 0;

    [[nodiscard]] virtual core::Status set_prepared_assets(
        const renderer::scene_bridge::PreparedRenderAssets& assets) noexcept = 0;
    [[nodiscard]] virtual core::Status clear_prepared_assets() noexcept = 0;
    [[nodiscard]] virtual core::Status attach_scene(scene::Scene& scene) noexcept = 0;
    [[nodiscard]] virtual core::Status detach_scene() noexcept = 0;

    [[nodiscard]] virtual core::Status set_ui_vertices(
        std::span<const editor::UiVertex> vertices) noexcept = 0;
    [[nodiscard]] virtual core::Status read_metrics(
        renderer::metrics::FrameTimingReport& report) noexcept = 0;

    virtual void begin_metrics() noexcept = 0;
    [[nodiscard]] virtual core::Status set_visibility_mode(
        renderer::gpu_culling::VisibilityMode mode) noexcept = 0;
    [[nodiscard]] virtual core::Status set_renderer_quality(
        renderer::quality::RendererQuality quality) noexcept = 0;
    [[nodiscard]] virtual core::Status set_vram_budget(core::u64 budget_bytes) noexcept = 0;
    [[nodiscard]] virtual core::Status set_procedural_workload(
        core::u32 instance_count) noexcept = 0;
    virtual void print_metrics() noexcept = 0;
    [[nodiscard]] virtual core::Status run_renderer_benchmark(
        bool use_gpu_culling) noexcept = 0;
};

[[nodiscard]] RendererBackend* create_vulkan_backend() noexcept;
[[nodiscard]] RendererBackend* create_d3d12_backend() noexcept;
[[nodiscard]] RendererBackend* create_metal_backend() noexcept;

} // namespace gameengine::rhi
