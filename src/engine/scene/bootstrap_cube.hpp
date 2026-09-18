#pragma once

#include <array>
#include <cstdint>

#include "engine/math/math.hpp"

namespace gameengine::scene {

struct ColoredVertex final {
    math::Vec3 position{};
    math::Vec3 color{};
};

inline constexpr std::array<ColoredVertex, 24> bootstrap_cube_vertices = {
    // Front (+Z)
    ColoredVertex{{-1.0F, -1.0F, 1.0F}, {1.0F, 0.2F, 0.2F}},
    ColoredVertex{{1.0F, -1.0F, 1.0F}, {1.0F, 0.2F, 0.2F}},
    ColoredVertex{{1.0F, 1.0F, 1.0F}, {1.0F, 0.2F, 0.2F}},
    ColoredVertex{{-1.0F, 1.0F, 1.0F}, {1.0F, 0.2F, 0.2F}},
    // Back (-Z)
    ColoredVertex{{1.0F, -1.0F, -1.0F}, {0.2F, 1.0F, 0.2F}},
    ColoredVertex{{-1.0F, -1.0F, -1.0F}, {0.2F, 1.0F, 0.2F}},
    ColoredVertex{{-1.0F, 1.0F, -1.0F}, {0.2F, 1.0F, 0.2F}},
    ColoredVertex{{1.0F, 1.0F, -1.0F}, {0.2F, 1.0F, 0.2F}},
    // Left (-X)
    ColoredVertex{{-1.0F, -1.0F, -1.0F}, {0.2F, 0.4F, 1.0F}},
    ColoredVertex{{-1.0F, -1.0F, 1.0F}, {0.2F, 0.4F, 1.0F}},
    ColoredVertex{{-1.0F, 1.0F, 1.0F}, {0.2F, 0.4F, 1.0F}},
    ColoredVertex{{-1.0F, 1.0F, -1.0F}, {0.2F, 0.4F, 1.0F}},
    // Right (+X)
    ColoredVertex{{1.0F, -1.0F, 1.0F}, {1.0F, 0.8F, 0.2F}},
    ColoredVertex{{1.0F, -1.0F, -1.0F}, {1.0F, 0.8F, 0.2F}},
    ColoredVertex{{1.0F, 1.0F, -1.0F}, {1.0F, 0.8F, 0.2F}},
    ColoredVertex{{1.0F, 1.0F, 1.0F}, {1.0F, 0.8F, 0.2F}},
    // Top (+Y)
    ColoredVertex{{-1.0F, 1.0F, 1.0F}, {0.8F, 0.2F, 1.0F}},
    ColoredVertex{{1.0F, 1.0F, 1.0F}, {0.8F, 0.2F, 1.0F}},
    ColoredVertex{{1.0F, 1.0F, -1.0F}, {0.8F, 0.2F, 1.0F}},
    ColoredVertex{{-1.0F, 1.0F, -1.0F}, {0.8F, 0.2F, 1.0F}},
    // Bottom (-Y)
    ColoredVertex{{-1.0F, -1.0F, -1.0F}, {0.2F, 0.9F, 1.0F}},
    ColoredVertex{{1.0F, -1.0F, -1.0F}, {0.2F, 0.9F, 1.0F}},
    ColoredVertex{{1.0F, -1.0F, 1.0F}, {0.2F, 0.9F, 1.0F}},
    ColoredVertex{{-1.0F, -1.0F, 1.0F}, {0.2F, 0.9F, 1.0F}},
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
