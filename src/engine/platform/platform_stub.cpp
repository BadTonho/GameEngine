#include "engine/platform/platform.hpp"

namespace gameengine::platform {

Platform::~Platform() noexcept
{
    shutdown();
}

core::Status Platform::initialize() noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

void Platform::shutdown() noexcept
{
    initialized_ = false;
    window_created_ = false;
    should_close_ = false;
    width_ = 0;
    height_ = 0;
    native_display_ = 0;
    native_window_ = 0;
    native_close_atom_ = 0;
    input_state_.reset();
}

core::Status Platform::create_window(const WindowDescription&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

void Platform::destroy_window() noexcept
{
    shutdown();
}

void Platform::poll_events(EventCallback, void*) noexcept {}

void Platform::dispatch_event(const input::Event&, EventCallback, void*) noexcept {}

} // namespace gameengine::platform
