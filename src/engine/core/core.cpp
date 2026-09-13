#include "engine/core/core.hpp"

namespace gameengine::core {

Status Core::initialize() noexcept
{
    if (initialized_) {
        return Status{ErrorCode::already_initialized};
    }

    initialized_ = true;
    return Status{};
}

void Core::shutdown() noexcept
{
    initialized_ = false;
}

} // namespace gameengine::core
