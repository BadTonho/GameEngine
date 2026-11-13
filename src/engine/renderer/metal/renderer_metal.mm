#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <new>

#include "engine/renderer/metal/metal_shaders.hpp"
#include "engine/rhi/rhi_backend.hpp"

namespace gameengine::rhi {

struct MetalBackend final : public RendererBackend {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> command_queue = nil;
    CAMetalLayer* metal_layer = nil;
    id<MTLLibrary> shader_library = nil;
    core::u64 vram_budget_bytes = 512ULL * 1024ULL * 1024ULL;

    [[nodiscard]] core::Status initialize(platform::NativeWindowHandles handles,
                                           platform::WindowSize size,
                                           const RendererConfiguration&) noexcept override
    {
        if (!handles.valid() || size.width == 0U || size.height == 0U) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            return core::Status{core::ErrorCode::unsupported_platform};
        }
        command_queue = [device newCommandQueue];
        if (command_queue == nil) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        metal_layer = (__bridge CAMetalLayer*)reinterpret_cast<void*>(handles.window);
        if (metal_layer != nil) {
            metal_layer.device = device;
            metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            metal_layer.drawableSize = CGSizeMake(size.width, size.height);
        }
        NSString* source = [[NSString alloc] initWithBytes:metal::bootstrap::triangle_vertex_msl.data()
                                                    length:metal::bootstrap::triangle_vertex_msl.size()
                                                  encoding:NSUTF8StringEncoding];
        NSError* error = nil;
        shader_library = [device newLibraryWithSource:source options:nil error:&error];
        if (shader_library == nil) {
            return core::Status{core::ErrorCode::allocation_failed};
        }
        return core::Status{};
    }

    [[nodiscard]] core::Status render_frame(platform::WindowSize size) noexcept override
    {
        if (device == nil || command_queue == nil) {
            return core::Status{core::ErrorCode::not_initialized};
        }
        if (metal_layer == nil || size.width == 0U || size.height == 0U) {
            return core::Status{};
        }
        metal_layer.drawableSize = CGSizeMake(size.width, size.height);
        id<CAMetalDrawable> drawable = [metal_layer nextDrawable];
        if (drawable == nil) {
            return core::Status{};
        }
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.06, 0.08, 0.12, 1.0);
        id<MTLCommandBuffer> cmd = [command_queue commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [cmd renderCommandEncoderWithDescriptor:pass];
        [encoder endEncoding];
        [cmd presentDrawable:drawable];
        [cmd commit];
        return core::Status{};
    }

    [[nodiscard]] core::Status reload_shaders() noexcept override { return core::Status{}; }
    [[nodiscard]] core::Status create_buffer(const BufferDescription&, BufferHandle&) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status upload_buffer(BufferHandle, std::span<const std::byte>) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status destroy_buffer(BufferHandle) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status create_image(const ImageDescription&, ImageHandle&) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status upload_image(ImageHandle, std::span<const std::byte>) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status destroy_image(ImageHandle) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status create_sampler(const SamplerDescription&, SamplerHandle&) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status destroy_sampler(SamplerHandle) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status create_graphics_pipeline(
        const GraphicsPipelineDescription&,
        PipelineHandle&) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }
    [[nodiscard]] core::Status destroy_pipeline(PipelineHandle) noexcept override
    {
        return core::Status{core::ErrorCode::unsupported_platform};
    }

    void shutdown() noexcept override
    {
        shader_library = nil;
        command_queue = nil;
        device = nil;
        metal_layer = nil;
    }

    [[nodiscard]] BackendCapabilities capabilities() const noexcept override
    {
        BackendCapabilities caps = query_static_backend_capabilities(BackendKind::metal);
        caps.compiled = true;
        caps.max_vram_budget_bytes = vram_budget_bytes;
        return caps;
    }

    [[nodiscard]] core::Status set_prepared_assets(
        const renderer::scene_bridge::PreparedRenderAssets&) noexcept override
    {
        return core::Status{};
    }
    [[nodiscard]] core::Status clear_prepared_assets() noexcept override { return core::Status{}; }
    [[nodiscard]] core::Status attach_scene(scene::Scene&) noexcept override { return core::Status{}; }
    [[nodiscard]] core::Status detach_scene() noexcept override { return core::Status{}; }
    [[nodiscard]] core::Status set_ui_vertices(
        std::span<const editor::UiVertex>) noexcept override
    {
        return core::Status{};
    }
    [[nodiscard]] core::Status read_metrics(
        renderer::metrics::FrameTimingReport& report) noexcept override
    {
        report = {};
        return core::Status{};
    }
    void begin_metrics() noexcept override {}
    [[nodiscard]] core::Status set_visibility_mode(
        renderer::gpu_culling::VisibilityMode) noexcept override
    {
        return core::Status{};
    }
    [[nodiscard]] core::Status set_renderer_quality(
        renderer::quality::RendererQuality) noexcept override
    {
        return core::Status{};
    }
    [[nodiscard]] core::Status set_vram_budget(core::u64 budget_bytes) noexcept override
    {
        vram_budget_bytes = budget_bytes;
        return core::Status{};
    }
    [[nodiscard]] core::Status set_procedural_workload(core::u32) noexcept override
    {
        return core::Status{};
    }
    void print_metrics() noexcept override {}
    [[nodiscard]] core::Status run_renderer_benchmark(bool) noexcept override
    {
        return core::Status{};
    }
};

RendererBackend* create_metal_backend() noexcept
{
    return new (std::nothrow) MetalBackend();
}

} // namespace gameengine::rhi

#else

#include "engine/rhi/rhi_backend.hpp"

namespace gameengine::rhi {

RendererBackend* create_metal_backend() noexcept
{
    return nullptr;
}

} // namespace gameengine::rhi

#endif
