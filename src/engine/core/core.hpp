#pragma once

namespace gameengine::core {

class Core final {
public:
    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace gameengine::core
