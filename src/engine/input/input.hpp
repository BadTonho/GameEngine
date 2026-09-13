#pragma once

#include <array>

#include "engine/core/types.hpp"

namespace gameengine::input {

enum class EventType : core::u8 {
    none = 0,
    quit_requested,
    window_resized,
    key_pressed,
    key_released,
    mouse_button_pressed,
    mouse_button_released,
    mouse_moved,
};

enum class KeyCode : core::u16 {
    unknown = 0,
    escape,
    enter,
    tab,
    backspace,
    space,
    left,
    right,
    up,
    down,
    shift,
    control,
    alt,
    super,
    a,
    b,
    c,
    d,
    e,
    f,
    g,
    h,
    i,
    j,
    k,
    l,
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t,
    u,
    v,
    w,
    x,
    y,
    z,
    digit_0,
    digit_1,
    digit_2,
    digit_3,
    digit_4,
    digit_5,
    digit_6,
    digit_7,
    digit_8,
    digit_9,
    count,
};

enum class MouseButton : core::u8 {
    unknown = 0,
    left,
    right,
    middle,
    x1,
    x2,
    count,
};

struct Event final {
    EventType type = EventType::none;
    KeyCode key = KeyCode::unknown;
    MouseButton mouse_button = MouseButton::unknown;
    core::i32 x = 0;
    core::i32 y = 0;
    core::u32 width = 0;
    core::u32 height = 0;
    bool repeated = false;
};

class InputState final {
public:
    void reset() noexcept
    {
        keys_.fill(false);
        mouse_buttons_.fill(false);
        mouse_x_ = 0;
        mouse_y_ = 0;
    }

    void apply(const Event& event) noexcept
    {
        switch (event.type) {
        case EventType::key_pressed:
            set_key(event.key, true);
            break;
        case EventType::key_released:
            set_key(event.key, false);
            break;
        case EventType::mouse_button_pressed:
            set_mouse_button(event.mouse_button, true);
            break;
        case EventType::mouse_button_released:
            set_mouse_button(event.mouse_button, false);
            break;
        case EventType::mouse_moved:
            mouse_x_ = event.x;
            mouse_y_ = event.y;
            break;
        case EventType::none:
        case EventType::quit_requested:
        case EventType::window_resized:
            break;
        }
    }

    [[nodiscard]] bool is_key_down(KeyCode key) const noexcept
    {
        const core::usize index = static_cast<core::usize>(key);
        return index > 0 && index < keys_.size() && keys_[index];
    }

    [[nodiscard]] bool is_mouse_button_down(MouseButton button) const noexcept
    {
        const core::usize index = static_cast<core::usize>(button);
        return index > 0 && index < mouse_buttons_.size() && mouse_buttons_[index];
    }

    [[nodiscard]] core::i32 mouse_x() const noexcept { return mouse_x_; }
    [[nodiscard]] core::i32 mouse_y() const noexcept { return mouse_y_; }

private:
    void set_key(KeyCode key, bool down) noexcept
    {
        const core::usize index = static_cast<core::usize>(key);
        if (index > 0 && index < keys_.size()) {
            keys_[index] = down;
        }
    }

    void set_mouse_button(MouseButton button, bool down) noexcept
    {
        const core::usize index = static_cast<core::usize>(button);
        if (index > 0 && index < mouse_buttons_.size()) {
            mouse_buttons_[index] = down;
        }
    }

    std::array<bool, static_cast<core::usize>(KeyCode::count)> keys_{};
    std::array<bool, static_cast<core::usize>(MouseButton::count)> mouse_buttons_{};
    core::i32 mouse_x_ = 0;
    core::i32 mouse_y_ = 0;
};

} // namespace gameengine::input
