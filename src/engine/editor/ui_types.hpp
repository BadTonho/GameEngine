#pragma once

#include <array>

#include "engine/core/types.hpp"

namespace gameengine::editor {

struct UiVertex final {
    std::array<core::f32, 2> position{};
    std::array<core::f32, 2> uv{};
    std::array<core::f32, 4> color{};
};

static_assert(sizeof(UiVertex) == 32U);

} // namespace gameengine::editor
