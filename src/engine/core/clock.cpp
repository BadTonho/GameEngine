#include "engine/core/clock.hpp"

#include <algorithm>

namespace gameengine::core {

void Clock::reset() noexcept
{
    started_ = false;
    last_tick_ = TimePoint{};
}

f64 Clock::tick() noexcept
{
    return tick_at(std::chrono::steady_clock::now());
}

f64 Clock::tick_at(TimePoint now) noexcept
{
    if (!started_) {
        last_tick_ = now;
        started_ = true;
        return 0.0;
    }

    const auto elapsed = now - last_tick_;
    last_tick_ = now;

    const f64 seconds = std::chrono::duration<f64>(elapsed).count();
    return std::clamp(seconds, 0.0, max_delta_seconds);
}

} // namespace gameengine::core
