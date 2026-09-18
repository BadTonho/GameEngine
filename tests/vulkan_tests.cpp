#include "engine/platform/platform.hpp"
#include "engine/rhi/rhi.hpp"

#include <array>
#include <cstddef>
#include <cstdio>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif

namespace {

[[nodiscard]] std::FILE* open_file(const char* path, const char* mode) noexcept
{
#if defined(_WIN32)
    std::FILE* file = nullptr;
    return fopen_s(&file, path, mode) == 0 ? file : nullptr;
#else
    return std::fopen(path, mode);
#endif
}

} // namespace

int main()
{
    constexpr const char* cache_path = "gameengine-vulkan-test.pipeline.cache";
    std::remove(cache_path);
    std::remove("gameengine-vulkan-test.pipeline.cache.tmp");

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

    std::array<char, 513> oversized_cache_path{};
    oversized_cache_path.fill('x');
    oversized_cache_path.back() = '\0';
    gameengine::rhi::Renderer invalid_path_renderer;
    const gameengine::rhi::RendererConfiguration invalid_path_configuration{
        .pipeline_cache_path = oversized_cache_path.data(),
        .enable_shader_hot_reload = false,
    };
    if (invalid_path_renderer.initialize(platform, invalid_path_configuration).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 3;
    }

    gameengine::rhi::Renderer renderer;
    const gameengine::rhi::RendererConfiguration cache_configuration{
        .pipeline_cache_path = cache_path,
        .enable_shader_hot_reload = false,
    };
    if (!renderer.initialize(platform, cache_configuration).ok()) {
        return 4;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 5;
    }

    gameengine::rhi::BufferHandle buffer;
    if (renderer.create_buffer({.size = 0}, buffer).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 6;
    }
    if (!renderer.create_buffer({.size = 16}, buffer).ok() || !buffer.valid()) {
        return 7;
    }
    const std::array<std::byte, 16> buffer_data{};
    if (!renderer.upload_buffer(buffer, buffer_data).ok()) {
        return 8;
    }

    gameengine::rhi::ImageHandle image;
    if (renderer.create_image({.width = 0, .height = 1}, image).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 9;
    }
    if (!renderer.create_image({.width = 1, .height = 1}, image).ok() || !image.valid()) {
        return 10;
    }
    const std::array<std::byte, 4> pixel = {
        std::byte{0xff},
        std::byte{0x00},
        std::byte{0xff},
        std::byte{0xff},
    };
    if (!renderer.upload_image(image, pixel).ok()) {
        return 11;
    }

    gameengine::rhi::SamplerHandle sampler;
    if (!renderer.create_sampler({}, sampler).ok() || !sampler.valid()) {
        return 12;
    }

    gameengine::rhi::PipelineHandle pipeline;
    if (!renderer.create_graphics_pipeline({}, pipeline).ok() || !pipeline.valid()) {
        return 13;
    }
    if (!renderer.destroy_buffer(buffer).ok() ||
        renderer.destroy_buffer(buffer).code != gameengine::core::ErrorCode::invalid_argument ||
        !renderer.destroy_image(image).ok() || !renderer.destroy_sampler(sampler).ok() ||
        !renderer.destroy_pipeline(pipeline).ok()) {
        return 14;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 15;
    }
    if (renderer.upload_buffer(buffer, buffer_data).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 16;
    }

    const auto handles = platform.native_window_handles();
#if defined(_WIN32)
    const auto hwnd = reinterpret_cast<HWND>(handles.window);
    RECT rect{0, 0, 640, 480};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    platform.poll_events();
#else
    auto* display = reinterpret_cast<Display*>(handles.display);
    const auto window = static_cast<::Window>(handles.window);
    XResizeWindow(display, window, 640, 480);
    XFlush(display);
    XSync(display, False);
    platform.poll_events();
#endif

    const auto size = platform.window_size();
    if (size.width != 640 || size.height != 480) {
        return 17;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 18;
    }
    if (renderer.reload_shaders().code !=
        gameengine::core::ErrorCode::shader_reload_disabled) {
        renderer.shutdown();
        std::remove(cache_path);
        return 19;
    }

    renderer.shutdown();
    std::FILE* cache_file = open_file(cache_path, "rb");
    const bool cache_closed = cache_file != nullptr && std::fclose(cache_file) == 0;
    if (!cache_closed) {
        std::remove(cache_path);
        return 20;
    }

    std::FILE* incompatible_file = open_file(cache_path, "r+b");
    const bool incompatible_written = incompatible_file != nullptr &&
                                      std::fseek(incompatible_file, 0, SEEK_SET) == 0 &&
                                      std::fwrite("BAD", 3, 1, incompatible_file) == 1;
    const bool incompatible_closed = incompatible_file != nullptr &&
                                     std::fclose(incompatible_file) == 0;
    if (!incompatible_written || !incompatible_closed) {
        std::remove(cache_path);
        return 21;
    }
    gameengine::rhi::Renderer incompatible_renderer;
    if (!incompatible_renderer.initialize(platform, cache_configuration).ok() ||
        !incompatible_renderer.render_frame(platform).ok()) {
        incompatible_renderer.shutdown();
        std::remove(cache_path);
        return 22;
    }
    incompatible_renderer.shutdown();

    gameengine::rhi::Renderer cached_renderer;
    if (!cached_renderer.initialize(platform, cache_configuration).ok() ||
        !cached_renderer.render_frame(platform).ok()) {
        cached_renderer.shutdown();
        std::remove(cache_path);
        return 23;
    }
    cached_renderer.shutdown();

    gameengine::rhi::Renderer reload_renderer;
    const gameengine::rhi::RendererConfiguration reload_configuration{
        .pipeline_cache_path = cache_path,
        .enable_shader_hot_reload = true,
    };
#ifdef NDEBUG
    if (reload_renderer.initialize(platform, reload_configuration).code !=
        gameengine::core::ErrorCode::shader_reload_disabled) {
        reload_renderer.shutdown();
        std::remove(cache_path);
        return 24;
    }
#else
    if (!reload_renderer.initialize(platform, reload_configuration).ok() ||
        !reload_renderer.reload_shaders().ok() ||
        !reload_renderer.render_frame(platform).ok()) {
        reload_renderer.shutdown();
        std::remove(cache_path);
        return 24;
    }
#endif
    reload_renderer.shutdown();

    std::FILE* malformed = open_file(cache_path, "wb");
    const bool malformed_written = malformed != nullptr &&
                                   std::fwrite("bad", 3, 1, malformed) == 1;
    const bool malformed_closed = malformed != nullptr && std::fclose(malformed) == 0;
    if (!malformed_written || !malformed_closed) {
        std::remove(cache_path);
        return 25;
    }
    gameengine::rhi::Renderer invalid_cache_renderer;
    if (!invalid_cache_renderer.initialize(platform, cache_configuration).ok() ||
        !invalid_cache_renderer.render_frame(platform).ok()) {
        invalid_cache_renderer.shutdown();
        std::remove(cache_path);
        return 26;
    }
    invalid_cache_renderer.shutdown();
    std::remove(cache_path);
    std::remove("gameengine-vulkan-test.pipeline.cache.tmp");
    platform.shutdown();
    return 0;
}
