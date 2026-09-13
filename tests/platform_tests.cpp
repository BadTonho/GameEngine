#include "engine/platform/platform.hpp"

namespace {

struct EventCapture final {
    int quit_events = 0;
    int resize_events = 0;
};

void capture_event(const gameengine::input::Event& event, void* user_data) noexcept
{
    auto* capture = static_cast<EventCapture*>(user_data);
    if (event.type == gameengine::input::EventType::quit_requested) {
        ++capture->quit_events;
    } else if (event.type == gameengine::input::EventType::window_resized) {
        ++capture->resize_events;
    }
}

} // namespace

int main()
{
    gameengine::platform::Platform platform;
    if (!platform.initialize().ok()) {
        return 1;
    }

    if (platform.initialize().code != gameengine::core::ErrorCode::already_initialized) {
        return 2;
    }

    const gameengine::platform::WindowDescription description{
        .title = "GameEngine platform test",
        .width = 320,
        .height = 240,
        .resizable = false,
    };
    if (!platform.create_window(description).ok()) {
        return 3;
    }

    const auto size = platform.window_size();
    if (size.width != 320 || size.height != 240) {
        return 4;
    }

    EventCapture capture;
    platform.poll_events(capture_event, &capture);
    platform.request_close();
    if (!platform.should_close()) {
        return 5;
    }

    platform.destroy_window();
    platform.shutdown();
    platform.shutdown();

    if (platform.is_initialized() || platform.has_window()) {
        return 6;
    }

    return 0;
}
