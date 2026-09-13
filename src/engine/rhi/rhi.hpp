#pragma once

#include "engine/core/status.hpp"
#include "engine/platform/platform.hpp"

namespace gameengine::rhi {

class Renderer final {
public:
    Renderer() noexcept = default;
    ~Renderer() noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] core::Status initialize(const platform::Platform& platform) noexcept;
    [[nodiscard]] core::Status render_frame(const platform::Platform& platform) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

    struct Impl;

private:
    Impl* impl_ = nullptr;
    bool initialized_ = false;
};

} // namespace gameengine::rhi
