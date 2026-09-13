#pragma once

#include <string_view>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/input/input.hpp"

namespace gameengine::platform {

struct WindowDescription final {
    std::string_view title = "GameEngine";
    core::u32 width = 1280;
    core::u32 height = 720;
    bool resizable = true;
};

struct WindowSize final {
    core::u32 width = 0;
    core::u32 height = 0;
};

struct NativeWindowHandles final {
    std::uintptr_t display = 0;
    std::uintptr_t window = 0;

    [[nodiscard]] bool valid() const noexcept { return display != 0 && window != 0; }
};

using EventCallback = void (*)(const input::Event& event, void* user_data) noexcept;

class Platform final {
public:
    Platform() noexcept = default;
    ~Platform() noexcept;

    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    [[nodiscard]] core::Status initialize() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] core::Status create_window(const WindowDescription& description) noexcept;
    void destroy_window() noexcept;

    void poll_events(EventCallback callback = nullptr, void* user_data = nullptr) noexcept;
    void request_close() noexcept { should_close_ = true; }

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool has_window() const noexcept { return window_created_; }
    [[nodiscard]] bool should_close() const noexcept { return should_close_; }
    [[nodiscard]] WindowSize window_size() const noexcept { return {width_, height_}; }
    [[nodiscard]] NativeWindowHandles native_window_handles() const noexcept
    {
        return {native_display_, native_window_};
    }
    [[nodiscard]] const input::InputState& input() const noexcept { return input_state_; }

private:
    void dispatch_event(const input::Event& event, EventCallback callback, void* user_data) noexcept;

    bool initialized_ = false;
    bool window_created_ = false;
    bool should_close_ = false;
    core::u32 width_ = 0;
    core::u32 height_ = 0;
    input::InputState input_state_{};

    // Native handles remain opaque to the shared platform interface.
    std::uintptr_t native_display_ = 0;
    std::uintptr_t native_window_ = 0;
    std::uintptr_t native_close_atom_ = 0;
};

} // namespace gameengine::platform
