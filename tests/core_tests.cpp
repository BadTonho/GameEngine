#include "engine/core/core.hpp"
#include "engine/core/clock.hpp"
#include "engine/input/input.hpp"

#include <chrono>

int main()
{
    gameengine::core::Core core;

    if (core.is_initialized()) {
        return 1;
    }

    if (!core.initialize().ok()) {
        return 2;
    }

    if (!core.is_initialized()) {
        return 3;
    }

    if (core.initialize().code != gameengine::core::ErrorCode::already_initialized) {
        return 4;
    }

    core.shutdown();
    core.shutdown();

    if (core.is_initialized()) {
        return 5;
    }

    gameengine::input::InputState input;
    gameengine::input::Event key_down{};
    key_down.type = gameengine::input::EventType::key_pressed;
    key_down.key = gameengine::input::KeyCode::a;
    input.apply(key_down);
    if (!input.is_key_down(gameengine::input::KeyCode::a)) {
        return 6;
    }

    gameengine::input::Event key_up = key_down;
    key_up.type = gameengine::input::EventType::key_released;
    input.apply(key_up);
    if (input.is_key_down(gameengine::input::KeyCode::a)) {
        return 7;
    }

    gameengine::input::Event mouse_move{};
    mouse_move.type = gameengine::input::EventType::mouse_moved;
    mouse_move.x = 42;
    mouse_move.y = 24;
    input.apply(mouse_move);
    if (input.mouse_x() != 42 || input.mouse_y() != 24) {
        return 8;
    }

    gameengine::core::Clock clock;
    const auto start = gameengine::core::Clock::TimePoint{};
    if (clock.tick_at(start) != 0.0) {
        return 9;
    }

    const auto future = start + std::chrono::seconds(1);
    if (clock.tick_at(future) != gameengine::core::Clock::max_delta_seconds) {
        return 10;
    }

    const auto earlier = future - std::chrono::seconds(2);
    if (clock.tick_at(earlier) != 0.0) {
        return 11;
    }

    return 0;
}
