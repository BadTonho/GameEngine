#pragma once

#include <cstddef>
#include <span>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/math/math.hpp"
#include "engine/renderer/renderer_benchmark.hpp"

namespace gameengine::renderer::forward_plus {

constexpr core::u32 tile_width = 16U;
constexpr core::u32 tile_height = 16U;
constexpr core::u32 max_point_lights = benchmark::max_point_lights;

struct TileHeader final {
    core::u32 offset = 0;
    core::u32 count = 0;
};

static_assert(sizeof(TileHeader) == 8U);

[[nodiscard]] constexpr core::u32 tile_count(core::u32 width, core::u32 height) noexcept
{
    if (width == 0U || height == 0U) {
        return 0U;
    }
    return ((width + tile_width - 1U) / tile_width) *
           ((height + tile_height - 1U) / tile_height);
}

[[nodiscard]] constexpr core::u32 maximum_index_count(core::u32 width,
                                                      core::u32 height,
                                                      core::u32 light_count) noexcept
{
    return tile_count(width, height) * light_count;
}

[[nodiscard]] bool build_tile_light_lists(
    core::u32 width,
    core::u32 height,
    std::span<const benchmark::PointLight> lights,
    std::span<TileHeader> headers,
    std::span<core::u32> indices) noexcept;

[[nodiscard]] math::Mat4 make_shadow_matrix(const math::Vec3& direction) noexcept;

[[nodiscard]] core::usize cubemap_rgba8_size(core::u32 resolution,
                                             core::u32 mip_count) noexcept;

[[nodiscard]] bool generate_cubemap_rgba8(core::u32 resolution,
                                          core::u32 mip_count,
                                          std::span<std::byte> output) noexcept;

} // namespace gameengine::renderer::forward_plus
