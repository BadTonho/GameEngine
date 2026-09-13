#include "engine/core/core.hpp"

namespace gameengine::core {

bool Core::initialize() noexcept
{
    if (initialized_) {
        return false;
    }

    initialized_ = true;
    return true;
}

void Core::shutdown() noexcept
{
    initialized_ = false;
}

} // namespace gameengine::core
