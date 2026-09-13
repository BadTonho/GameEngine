#include "engine/core/core.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/diagnostics.hpp"
#include "engine/platform/platform.hpp"

#include <chrono>
#include <cstring>
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
    if (argc > 1 && !smoke_test) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "unknown argument; use --smoke-test");
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

    gameengine::core::Clock clock;
    do {
        platform.poll_events();
        const gameengine::core::f64 delta_seconds = clock.tick();
        (void)delta_seconds;

        if (smoke_test) {
            platform.request_close();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } while (!platform.should_close());

    platform.shutdown();
#else
    (void)argc;
    (void)argv;
#endif

    core.shutdown();
    return 0;
}
