#include "engine/platform/platform.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

struct EventTracker final {
    int quit_events = 0;
    int resize_events = 0;
    int key_pressed_events = 0;
    int key_released_events = 0;
    int mouse_moved_events = 0;
};

void on_event(const gameengine::input::Event& event, void* user_data) noexcept
{
    auto* tracker = static_cast<EventTracker*>(user_data);
    switch (event.type) {
    case gameengine::input::EventType::quit_requested:
        ++tracker->quit_events;
        break;
    case gameengine::input::EventType::window_resized:
        ++tracker->resize_events;
        break;
    case gameengine::input::EventType::key_pressed:
        ++tracker->key_pressed_events;
        break;
    case gameengine::input::EventType::key_released:
        ++tracker->key_released_events;
        break;
    case gameengine::input::EventType::mouse_moved:
        ++tracker->mouse_moved_events;
        break;
    default:
        break;
    }
}

} // namespace

int main()
{
    gameengine::platform::Platform platform;

    // 1. Initial state
    if (platform.is_initialized() || platform.has_window() || platform.should_close()) {
        return 1;
    }

    // 2. Initialize
    if (!platform.initialize().ok()) {
        return 2;
    }
    if (!platform.is_initialized()) {
        return 3;
    }

    // Double initialize should return already_initialized
    if (platform.initialize().code != gameengine::core::ErrorCode::already_initialized) {
        return 4;
    }

    // 3. Invalid window arguments
    if (platform.create_window({.width = 0, .height = 100}).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 5;
    }
    if (platform.create_window({.width = 100, .height = 0}).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 6;
    }

    // 4. Valid window creation
    const gameengine::platform::WindowDescription desc{
        .title = "Win32 Test Window",
        .width = 400,
        .height = 300,
        .resizable = false,
    };
    if (!platform.create_window(desc).ok()) {
        return 7;
    }
    if (!platform.has_window()) {
        return 8;
    }

    const auto size = platform.window_size();
    if (size.width != 400 || size.height != 300) {
        return 9;
    }

    const auto handles = platform.native_window_handles();
    if (!handles.valid() || handles.display == 0 || handles.window == 0) {
        return 10;
    }

    // 5. Polling events and input testing
    EventTracker tracker{};
    platform.poll_events(on_event, &tracker);

    const auto hwnd = reinterpret_cast<HWND>(handles.window);

    // Simulate key events via PostMessage
    PostMessageW(hwnd, WM_KEYDOWN, VK_SPACE, 0);
    PostMessageW(hwnd, WM_KEYUP, VK_SPACE, 0);
    // Simulate mouse move
    PostMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(50, 60));

    platform.poll_events(on_event, &tracker);

    if (tracker.key_pressed_events != 1 || tracker.key_released_events != 1) {
        return 11;
    }
    if (tracker.mouse_moved_events != 1) {
        return 12;
    }

    // 6. Request close
    platform.request_close();
    if (!platform.should_close()) {
        return 13;
    }

    // 7. Destroy window
    platform.destroy_window();
    if (platform.has_window() || platform.native_window_handles().valid()) {
        return 14;
    }

    // 8. Shutdown (idempotent)
    platform.shutdown();
    platform.shutdown();

    if (platform.is_initialized()) {
        return 15;
    }

    return 0;
}
