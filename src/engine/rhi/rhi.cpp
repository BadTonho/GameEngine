#include "engine/rhi/rhi.hpp"
#include "engine/rhi/rhi_backend.hpp"
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/scene_bridge.hpp"

#include <new>

namespace gameengine::rhi {

namespace {

[[nodiscard]] RendererBackend* instantiate_backend(BackendKind requested) noexcept
{
    const BackendKind resolved =
        requested == BackendKind::auto_select ? resolve_default_backend() : requested;
    switch (resolved) {
    case BackendKind::vulkan:
        return create_vulkan_backend();
    case BackendKind::d3d12:
        return create_d3d12_backend();
    case BackendKind::metal:
        return create_metal_backend();
    case BackendKind::auto_select:
        return nullptr;
    }
    return nullptr;
}

} // namespace

struct Renderer::Impl final {
    RendererBackend* backend = nullptr;
    BackendKind requested_backend = BackendKind::auto_select;
};

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

    const BackendKind target_backend = configuration.backend == BackendKind::auto_select
                                           ? resolve_default_backend()
                                           : configuration.backend;
    if (!is_backend_compiled(target_backend)) {
        return core::Status{core::ErrorCode::unsupported_platform};
    }

    RendererBackend* backend = instantiate_backend(target_backend);
    if (backend == nullptr) {
        return core::Status{core::ErrorCode::unsupported_platform};
    }

    Impl* impl = new (std::nothrow) Impl();
    if (impl == nullptr) {
        delete backend;
        return core::Status{core::ErrorCode::allocation_failed};
    }
    impl->backend = backend;
    impl->requested_backend = target_backend;

    const core::Status status = backend->initialize(platform.native_window_handles(),
                                                    platform.window_size(),
                                                    configuration);
    if (!status) {
        backend->shutdown();
        delete backend;
        delete impl;
        return status;
    }

    impl_ = impl;
    initialized_ = true;
    return core::Status{};
}

core::Status Renderer::render_frame(const platform::Platform& platform) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!platform.is_initialized() || !platform.has_window()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return impl_->backend->render_frame(platform.window_size());
}

core::Status Renderer::reload_shaders() noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->reload_shaders();
}

core::Status Renderer::create_buffer(const BufferDescription& description,
                                     BufferHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->create_buffer(description, handle);
}

core::Status Renderer::upload_buffer(BufferHandle handle,
                                     std::span<const std::byte> data) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->upload_buffer(handle, data);
}

core::Status Renderer::destroy_buffer(BufferHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->destroy_buffer(handle);
}

core::Status Renderer::create_image(const ImageDescription& description,
                                    ImageHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->create_image(description, handle);
}

core::Status Renderer::upload_image(ImageHandle handle,
                                    std::span<const std::byte> data) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->upload_image(handle, data);
}

core::Status Renderer::destroy_image(ImageHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->destroy_image(handle);
}

core::Status Renderer::create_sampler(const SamplerDescription& description,
                                      SamplerHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->create_sampler(description, handle);
}

core::Status Renderer::destroy_sampler(SamplerHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->destroy_sampler(handle);
}

core::Status Renderer::create_graphics_pipeline(
    const GraphicsPipelineDescription& description,
    PipelineHandle& handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->create_graphics_pipeline(description, handle);
}

core::Status Renderer::destroy_pipeline(PipelineHandle handle) noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return impl_->backend->destroy_pipeline(handle);
}

void Renderer::shutdown() noexcept
{
    initialized_ = false;
    if (impl_ != nullptr) {
        if (impl_->backend != nullptr) {
            impl_->backend->shutdown();
            delete impl_->backend;
            impl_->backend = nullptr;
        }
        delete impl_;
        impl_ = nullptr;
    }
}

BackendKind Renderer::backend_kind() const noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return BackendKind::auto_select;
    }
    return impl_->backend->capabilities().backend;
}

BackendCapabilities Renderer::capabilities() const noexcept
{
    if (!initialized_ || impl_ == nullptr || impl_->backend == nullptr) {
        return BackendCapabilities{};
    }
    return impl_->backend->capabilities();
}

} // namespace gameengine::rhi

namespace gameengine::renderer::scene_bridge {

core::Status set_prepared_assets(const rhi::Renderer& renderer,
                                 const PreparedRenderAssets& assets) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->set_prepared_assets(assets);
}

core::Status clear_prepared_assets(const rhi::Renderer& renderer) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->clear_prepared_assets();
}

core::Status attach_scene(const rhi::Renderer& renderer, scene::Scene& scene) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->attach_scene(scene);
}

core::Status detach_scene(const rhi::Renderer& renderer) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->detach_scene();
}

} // namespace gameengine::renderer::scene_bridge

namespace gameengine::editor::renderer_bridge {

core::Status set_prepared_assets(
    const gameengine::rhi::Renderer& renderer,
    const gameengine::renderer::scene_bridge::PreparedRenderAssets& assets) noexcept
{
    return gameengine::renderer::scene_bridge::set_prepared_assets(renderer, assets);
}

core::Status clear_prepared_assets(const gameengine::rhi::Renderer& renderer) noexcept
{
    return gameengine::renderer::scene_bridge::clear_prepared_assets(renderer);
}

core::Status attach_scene(const gameengine::rhi::Renderer& renderer,
                          gameengine::scene::Scene& scene) noexcept
{
    return gameengine::renderer::scene_bridge::attach_scene(renderer, scene);
}

core::Status detach_scene(const gameengine::rhi::Renderer& renderer) noexcept
{
    return gameengine::renderer::scene_bridge::detach_scene(renderer);
}

core::Status set_ui_vertices(const gameengine::rhi::Renderer& renderer,
                             std::span<const gameengine::editor::UiVertex> vertices) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->set_ui_vertices(vertices);
}

core::Status read_metrics(const gameengine::rhi::Renderer& renderer,
                          gameengine::renderer::metrics::FrameTimingReport& report) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->read_metrics(report);
}

} // namespace gameengine::editor::renderer_bridge

namespace gameengine::renderer::diagnostics {

void begin_metrics(const gameengine::rhi::Renderer& renderer) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return;
    }
    renderer.impl_->backend->begin_metrics();
}

core::Status set_visibility_mode(const gameengine::rhi::Renderer& renderer,
                                 gpu_culling::VisibilityMode mode) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->set_visibility_mode(mode);
}

core::Status set_renderer_quality(const gameengine::rhi::Renderer& renderer,
                                  quality::RendererQuality quality) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->set_renderer_quality(quality);
}

core::Status set_vram_budget(const gameengine::rhi::Renderer& renderer,
                             core::u64 budget_bytes) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->set_vram_budget(budget_bytes);
}

core::Status set_procedural_workload(const gameengine::rhi::Renderer& renderer,
                                     core::u32 instance_count) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->set_procedural_workload(instance_count);
}

void print_metrics(const gameengine::rhi::Renderer& renderer) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return;
    }
    renderer.impl_->backend->print_metrics();
}

core::Status run_renderer_benchmark(const gameengine::rhi::Renderer& renderer,
                                    bool use_gpu_culling) noexcept
{
    if (!renderer.is_initialized() || renderer.impl_ == nullptr ||
        renderer.impl_->backend == nullptr) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    return renderer.impl_->backend->run_renderer_benchmark(use_gpu_culling);
}

} // namespace gameengine::renderer::diagnostics
