#include "engine/rhi/rhi.hpp"
#include "engine/renderer/renderer_metrics.hpp"

#include <cstdio>

namespace gameengine::rhi {

struct Renderer::Impl final {};

Renderer::~Renderer() noexcept
{
    shutdown();
}

core::Status Renderer::initialize(const platform::Platform&,
                                  const RendererConfiguration&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::render_frame(const platform::Platform&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::reload_shaders() noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::create_buffer(const BufferDescription&, BufferHandle&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::upload_buffer(BufferHandle, std::span<const std::byte>) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::destroy_buffer(BufferHandle) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::create_image(const ImageDescription&, ImageHandle&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::upload_image(ImageHandle, std::span<const std::byte>) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::destroy_image(ImageHandle) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::create_sampler(const SamplerDescription&, SamplerHandle&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::destroy_sampler(SamplerHandle) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::create_graphics_pipeline(const GraphicsPipelineDescription&,
                                                PipelineHandle&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::destroy_pipeline(PipelineHandle) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

void Renderer::shutdown() noexcept
{
    initialized_ = false;
    delete impl_;
    impl_ = nullptr;
}

} // namespace gameengine::rhi

namespace gameengine::renderer::diagnostics {

void begin_metrics(const gameengine::rhi::Renderer&) noexcept {}

core::Status set_procedural_workload(const gameengine::rhi::Renderer&,
                                     core::u32) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

void print_metrics(const gameengine::rhi::Renderer&) noexcept
{
    std::fprintf(stderr, "[gameengine] [info] renderer metrics unavailable\n");
}

} // namespace gameengine::renderer::diagnostics
