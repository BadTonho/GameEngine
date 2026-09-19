#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "engine/math/math.hpp"

namespace gameengine::scene {

struct TexturedVertex final {
    math::Vec3 position{};
    math::Vec3 normal{};
    math::Vec2 uv{};
};

static_assert(sizeof(TexturedVertex) == 32U);
static_assert(offsetof(TexturedVertex, normal) == sizeof(math::Vec3));
static_assert(offsetof(TexturedVertex, uv) == sizeof(math::Vec3) * 2U);

inline constexpr std::array<TexturedVertex, 24> bootstrap_cube_vertices = {
    // Front (+Z)
    TexturedVertex{{-1.0F, -1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F}},
    TexturedVertex{{1.0F, -1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F}},
    TexturedVertex{{1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F}},
    TexturedVertex{{-1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F}},
    // Back (-Z)
    TexturedVertex{{1.0F, -1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 1.0F}},
    TexturedVertex{{-1.0F, -1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {1.0F, 1.0F}},
    TexturedVertex{{-1.0F, 1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F}},
    TexturedVertex{{1.0F, 1.0F, -1.0F}, {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F}},
    // Left (-X)
    TexturedVertex{{-1.0F, -1.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}, {0.0F, 1.0F}},
    TexturedVertex{{-1.0F, -1.0F, 1.0F}, {-1.0F, 0.0F, 0.0F}, {1.0F, 1.0F}},
    TexturedVertex{{-1.0F, 1.0F, 1.0F}, {-1.0F, 0.0F, 0.0F}, {1.0F, 0.0F}},
    TexturedVertex{{-1.0F, 1.0F, -1.0F}, {-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F}},
    // Right (+X)
    TexturedVertex{{1.0F, -1.0F, 1.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F}},
    TexturedVertex{{1.0F, -1.0F, -1.0F}, {1.0F, 0.0F, 0.0F}, {1.0F, 1.0F}},
    TexturedVertex{{1.0F, 1.0F, -1.0F}, {1.0F, 0.0F, 0.0F}, {1.0F, 0.0F}},
    TexturedVertex{{1.0F, 1.0F, 1.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F}},
    // Top (+Y)
    TexturedVertex{{-1.0F, 1.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F}},
    TexturedVertex{{1.0F, 1.0F, 1.0F}, {0.0F, 1.0F, 0.0F}, {1.0F, 1.0F}},
    TexturedVertex{{1.0F, 1.0F, -1.0F}, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F}},
    TexturedVertex{{-1.0F, 1.0F, -1.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F}},
    // Bottom (-Y)
    TexturedVertex{{-1.0F, -1.0F, -1.0F}, {0.0F, -1.0F, 0.0F}, {0.0F, 1.0F}},
    TexturedVertex{{1.0F, -1.0F, -1.0F}, {0.0F, -1.0F, 0.0F}, {1.0F, 1.0F}},
    TexturedVertex{{1.0F, -1.0F, 1.0F}, {0.0F, -1.0F, 0.0F}, {1.0F, 0.0F}},
    TexturedVertex{{-1.0F, -1.0F, 1.0F}, {0.0F, -1.0F, 0.0F}, {0.0F, 0.0F}},
};

inline constexpr std::array<std::uint16_t, 36> bootstrap_cube_indices = {
    0, 1, 2, 2, 3, 0,       // Front
    4, 5, 6, 6, 7, 4,       // Back
    8, 9, 10, 10, 11, 8,    // Left
    12, 13, 14, 14, 15, 12, // Right
    16, 17, 18, 18, 19, 16, // Top
    20, 21, 22, 22, 23, 20, // Bottom
};

} // namespace gameengine::scene
