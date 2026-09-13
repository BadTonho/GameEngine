#include "engine/platform/platform.hpp"
#include "engine/rhi/rhi.hpp"

#include <array>
#include <cstddef>

#include <X11/Xlib.h>

int main()
{
    gameengine::platform::Platform platform;
    if (!platform.initialize().ok()) {
        return 1;
    }

    if (!platform.create_window({
            .title = "GameEngine Vulkan resize test",
            .width = 320,
            .height = 240,
            .resizable = true,
        })) {
        return 2;
    }

    gameengine::rhi::Renderer renderer;
    if (!renderer.initialize(platform).ok()) {
        return 3;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 4;
    }

    gameengine::rhi::BufferHandle buffer;
    if (renderer.create_buffer({.size = 0}, buffer).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 5;
    }
    if (!renderer.create_buffer({.size = 16}, buffer).ok() || !buffer.valid()) {
        return 6;
    }
    const std::array<std::byte, 16> buffer_data{};
    if (!renderer.upload_buffer(buffer, buffer_data).ok()) {
        return 7;
    }

    gameengine::rhi::ImageHandle image;
    if (renderer.create_image({.width = 0, .height = 1}, image).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 8;
    }
    if (!renderer.create_image({.width = 1, .height = 1}, image).ok() || !image.valid()) {
        return 9;
    }
    const std::array<std::byte, 4> pixel = {
        std::byte{0xff},
        std::byte{0x00},
        std::byte{0xff},
        std::byte{0xff},
    };
    if (!renderer.upload_image(image, pixel).ok()) {
        return 10;
    }

    gameengine::rhi::SamplerHandle sampler;
    if (!renderer.create_sampler({}, sampler).ok() || !sampler.valid()) {
        return 11;
    }

    gameengine::rhi::PipelineHandle pipeline;
    if (!renderer.create_graphics_pipeline({}, pipeline).ok() || !pipeline.valid()) {
        return 12;
    }
    if (!renderer.destroy_buffer(buffer).ok() ||
        renderer.destroy_buffer(buffer).code != gameengine::core::ErrorCode::invalid_argument ||
        !renderer.destroy_image(image).ok() || !renderer.destroy_sampler(sampler).ok() ||
        !renderer.destroy_pipeline(pipeline).ok()) {
        return 13;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 14;
    }
    if (renderer.upload_buffer(buffer, buffer_data).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 15;
    }

    const auto handles = platform.native_window_handles();
    auto* display = reinterpret_cast<Display*>(handles.display);
    const auto window = static_cast<::Window>(handles.window);
    XResizeWindow(display, window, 640, 480);
    XFlush(display);
    XSync(display, False);
    platform.poll_events();

    const auto size = platform.window_size();
    if (size.width != 640 || size.height != 480) {
        return 16;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 17;
    }

    renderer.shutdown();
    platform.shutdown();
    return 0;
}
