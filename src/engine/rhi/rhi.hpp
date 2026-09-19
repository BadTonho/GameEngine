#pragma once

#include <cstddef>
#include <span>

#include "engine/core/status.hpp"
#include "engine/platform/platform.hpp"

namespace gameengine::rhi {
class Renderer;
}

namespace gameengine::renderer::gpu_culling {
enum class VisibilityMode : core::u8;
}

namespace gameengine::renderer::diagnostics {

void begin_metrics(const gameengine::rhi::Renderer& renderer) noexcept;
[[nodiscard]] core::Status set_visibility_mode(
    const gameengine::rhi::Renderer& renderer,
    gpu_culling::VisibilityMode mode) noexcept;
[[nodiscard]] core::Status set_procedural_workload(const gameengine::rhi::Renderer& renderer,
                                                   core::u32 instance_count) noexcept;
void print_metrics(const gameengine::rhi::Renderer& renderer) noexcept;

} // namespace gameengine::renderer::diagnostics

namespace gameengine::rhi {

constexpr core::u32 invalid_handle_index = 0xffffffffU;

struct BufferHandle final {
    core::u32 index = invalid_handle_index;
    core::u32 generation = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return index != invalid_handle_index && generation != 0;
    }
};

struct ImageHandle final {
    core::u32 index = invalid_handle_index;
    core::u32 generation = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return index != invalid_handle_index && generation != 0;
    }
};

struct SamplerHandle final {
    core::u32 index = invalid_handle_index;
    core::u32 generation = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return index != invalid_handle_index && generation != 0;
    }
};

struct PipelineHandle final {
    core::u32 index = invalid_handle_index;
    core::u32 generation = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return index != invalid_handle_index && generation != 0;
    }
};

enum class BufferUsage : core::u8 {
    vertex = 0,
    index,
};

enum class ImageFormat : core::u8 {
    rgba8_unorm = 0,
};

struct BufferDescription final {
    core::usize size = 0;
    BufferUsage usage = BufferUsage::vertex;
};

struct ImageDescription final {
    core::u32 width = 0;
    core::u32 height = 0;
    ImageFormat format = ImageFormat::rgba8_unorm;
};

enum class SamplerFilter : core::u8 {
    nearest = 0,
    linear,
};

struct SamplerDescription final {
    SamplerFilter min_filter = SamplerFilter::linear;
    SamplerFilter mag_filter = SamplerFilter::linear;
};

enum class PipelineVertexLayout : core::u8 {
    position3_color3 = 0,
};

struct GraphicsPipelineDescription final {
    PipelineVertexLayout vertex_layout = PipelineVertexLayout::position3_color3;
};

struct RendererConfiguration final {
    const char* pipeline_cache_path = "gameengine.pipeline.cache";
    bool enable_shader_hot_reload = false;
};

class Renderer final {
public:
    Renderer() noexcept = default;
    ~Renderer() noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] core::Status initialize(
        const platform::Platform& platform,
        const RendererConfiguration& configuration = {}) noexcept;
    [[nodiscard]] core::Status render_frame(const platform::Platform& platform) noexcept;
    [[nodiscard]] core::Status reload_shaders() noexcept;

    [[nodiscard]] core::Status create_buffer(const BufferDescription& description,
                                              BufferHandle& handle) noexcept;
    [[nodiscard]] core::Status upload_buffer(BufferHandle handle,
                                              std::span<const std::byte> data) noexcept;
    [[nodiscard]] core::Status destroy_buffer(BufferHandle handle) noexcept;

    [[nodiscard]] core::Status create_image(const ImageDescription& description,
                                             ImageHandle& handle) noexcept;
    [[nodiscard]] core::Status upload_image(ImageHandle handle,
                                             std::span<const std::byte> data) noexcept;
    [[nodiscard]] core::Status destroy_image(ImageHandle handle) noexcept;

    [[nodiscard]] core::Status create_sampler(const SamplerDescription& description,
                                              SamplerHandle& handle) noexcept;
    [[nodiscard]] core::Status destroy_sampler(SamplerHandle handle) noexcept;

    [[nodiscard]] core::Status create_graphics_pipeline(
        const GraphicsPipelineDescription& description,
        PipelineHandle& handle) noexcept;
    [[nodiscard]] core::Status destroy_pipeline(PipelineHandle handle) noexcept;

    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

    struct Impl;

private:
    friend void renderer::diagnostics::begin_metrics(const Renderer& renderer) noexcept;
    friend core::Status renderer::diagnostics::set_visibility_mode(
        const Renderer& renderer,
        renderer::gpu_culling::VisibilityMode mode) noexcept;
    friend core::Status renderer::diagnostics::set_procedural_workload(
        const Renderer& renderer,
        core::u32 instance_count) noexcept;
    friend void renderer::diagnostics::print_metrics(const Renderer& renderer) noexcept;

    Impl* impl_ = nullptr;
    bool initialized_ = false;
};

} // namespace gameengine::rhi
