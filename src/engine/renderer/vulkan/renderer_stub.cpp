#include "engine/rhi/rhi.hpp"

namespace gameengine::rhi {

struct Renderer::Impl final {};

Renderer::~Renderer() noexcept
{
    shutdown();
}

core::Status Renderer::initialize(const platform::Platform&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

core::Status Renderer::render_frame(const platform::Platform&) noexcept
{
    return core::Status{core::ErrorCode::unsupported_platform};
}

void Renderer::shutdown() noexcept
{
    initialized_ = false;
    delete impl_;
    impl_ = nullptr;
}

} // namespace gameengine::rhi
