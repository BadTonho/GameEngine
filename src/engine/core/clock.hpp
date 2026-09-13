#pragma once

#include <chrono>

#include "engine/core/types.hpp"

namespace gameengine::core {

class Clock final {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    static constexpr f64 max_delta_seconds = 0.25;

    void reset() noexcept;

    [[nodiscard]] f64 tick() noexcept;
    [[nodiscard]] f64 tick_at(TimePoint now) noexcept;

private:
    TimePoint last_tick_{};
    bool started_ = false;
};

} // namespace gameengine::core
