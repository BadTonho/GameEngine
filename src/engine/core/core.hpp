#pragma once

#include "engine/core/status.hpp"

namespace gameengine::core {

class Core final {
public:
    [[nodiscard]] Status initialize() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace gameengine::core
