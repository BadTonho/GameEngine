#include "engine/core/core.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/diagnostics.hpp"
#include "engine/platform/platform.hpp"
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/rhi/rhi.hpp"

#include <chrono>
#include <cstring>
#include <cstdint>
#include <thread>

int main(int argc, char** argv)
{
    gameengine::core::Core core;

    const gameengine::core::Status core_status = core.initialize();
    if (!core_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(core_status.code));
        return 1;
    }

#if GAMEENGINE_PLATFORM_HAS_WINDOW
    const bool smoke_test = argc == 2 && std::strcmp(argv[1], "--smoke-test") == 0;
    const bool metrics_test = argc == 2 && std::strcmp(argv[1], "--metrics") == 0;
    if (argc > 1 && !smoke_test && !metrics_test) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "unknown argument; use --smoke-test or --metrics");
        core.shutdown();
        return 2;
    }

    gameengine::platform::Platform platform;
    const gameengine::core::Status platform_status = platform.initialize();
    if (!platform_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(platform_status.code));
        core.shutdown();
        return 3;
    }

    const gameengine::platform::WindowDescription window_description{};
    const gameengine::core::Status window_status = platform.create_window(window_description);
    if (!window_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(window_status.code));
        platform.shutdown();
        core.shutdown();
        return 4;
    }

#if GAMEENGINE_RENDERER_HAS_VULKAN
    gameengine::rhi::Renderer renderer;
    const gameengine::core::Status renderer_status = renderer.initialize(platform);
    if (!renderer_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(renderer_status.code));
        platform.shutdown();
        core.shutdown();
        return 5;
    }
#endif

    gameengine::core::Clock clock;
    std::uint32_t metrics_frame_count = 0;
    constexpr std::uint32_t metrics_warmup_frames = 30;
    constexpr std::uint32_t metrics_total_frames = 90;
    do {
        platform.poll_events();
        const gameengine::core::f64 delta_seconds = clock.tick();
        (void)delta_seconds;

#if GAMEENGINE_RENDERER_HAS_VULKAN
        if (metrics_test && metrics_frame_count == metrics_warmup_frames) {
            gameengine::renderer::diagnostics::begin_metrics(renderer);
        }
        const gameengine::core::Status frame_status = renderer.render_frame(platform);
        if (!frame_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(frame_status.code));
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
        if (metrics_test) {
            ++metrics_frame_count;
        }
#endif

        if (smoke_test) {
            platform.request_close();
        } else if (metrics_test && metrics_frame_count >= metrics_total_frames) {
            platform.request_close();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } while (!platform.should_close());

#if GAMEENGINE_RENDERER_HAS_VULKAN
    if (metrics_test) {
        gameengine::renderer::diagnostics::print_metrics(renderer);
    }
    renderer.shutdown();
#endif
    platform.shutdown();
#else
    (void)argc;
    (void)argv;
#endif

    core.shutdown();
    return 0;
}
