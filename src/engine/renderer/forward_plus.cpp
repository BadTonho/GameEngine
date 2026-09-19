#include "engine/renderer/forward_plus.hpp"

#include <algorithm>
#include <cmath>

namespace gameengine::renderer::forward_plus {

bool build_tile_light_lists(core::u32 width,
                            core::u32 height,
                            std::span<const benchmark::PointLight> lights,
                            std::span<TileHeader> headers,
                            std::span<core::u32> indices) noexcept
{
    const core::u32 count = tile_count(width, height);
    if (headers.size() < count || lights.size() > max_point_lights ||
        indices.size() < maximum_index_count(width, height,
                                              static_cast<core::u32>(lights.size()))) {
        return false;
    }
    const core::u32 columns = width == 0U ? 0U : (width + tile_width - 1U) / tile_width;
    const core::f32 inverse_width = width == 0U ? 0.0F : 2.0F / static_cast<core::f32>(width);
    const core::f32 inverse_height = height == 0U ? 0.0F : 2.0F / static_cast<core::f32>(height);
    core::u32 next_index = 0U;
    for (core::u32 tile = 0; tile < count; ++tile) {
        const core::u32 tile_x = tile % columns;
        const core::u32 tile_y = tile / columns;
        const core::f32 center_x = (static_cast<core::f32>(tile_x * tile_width) +
                                    static_cast<core::f32>(tile_width) * 0.5F) *
                                       inverse_width -
                                   1.0F;
        const core::f32 center_y = (static_cast<core::f32>(tile_y * tile_height) +
                                    static_cast<core::f32>(tile_height) * 0.5F) *
                                       inverse_height -
                                   1.0F;
        const core::u32 start = next_index;
        for (core::u32 light_index = 0;
             light_index < static_cast<core::u32>(lights.size());
             ++light_index) {
            const benchmark::PointLight& light = lights[light_index];
            const core::f32 projected_x = light.position.x * 0.02F;
            const core::f32 projected_y = light.position.y * 0.02F;
            const core::f32 radius = std::max(light.radius * 0.02F, 0.02F);
            if (std::fabs(projected_x - center_x) <= radius + inverse_width * tile_width * 0.5F &&
                std::fabs(projected_y - center_y) <= radius + inverse_height * tile_height * 0.5F) {
                indices[next_index++] = light_index;
            }
        }
        headers[tile] = {start, next_index - start};
    }
    return true;
}

math::Mat4 make_shadow_matrix(const math::Vec3& direction) noexcept
{
    const math::Vec3 light_direction = math::normalize(direction);
    const math::Vec3 eye = math::multiply(light_direction, -12.0F);
    const math::Mat4 view = math::look_at_rh(eye, {0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
    const math::Mat4 projection = math::perspective_rh_zo(1.2F, 1.0F, 0.1F, 40.0F);
    return math::multiply(projection, view);
}

core::usize cubemap_rgba8_size(core::u32 resolution, core::u32 mip_count) noexcept
{
    if (resolution == 0U || mip_count == 0U) {
        return 0U;
    }
    core::usize size = 0U;
    core::u32 extent = resolution;
    for (core::u32 mip = 0; mip < mip_count; ++mip) {
        size += static_cast<core::usize>(extent) * extent * 6U * 4U;
        extent = std::max(extent / 2U, 1U);
    }
    return size;
}

bool generate_cubemap_rgba8(core::u32 resolution,
                            core::u32 mip_count,
                            std::span<std::byte> output) noexcept
{
    const core::usize required_size = cubemap_rgba8_size(resolution, mip_count);
    if (required_size == 0U || output.size() < required_size) {
        return false;
    }
    core::usize cursor = 0U;
    core::u32 extent = resolution;
    for (core::u32 mip = 0; mip < mip_count; ++mip) {
        for (core::u32 face = 0; face < 6U; ++face) {
            for (core::u32 y = 0; y < extent; ++y) {
                for (core::u32 x = 0; x < extent; ++x) {
                    const auto value = static_cast<std::uint8_t>(
                        (x * 13U + y * 17U + face * 29U + mip * 7U) & 0xffU);
                    output[cursor++] = std::byte{value};
                    output[cursor++] = std::byte{static_cast<std::uint8_t>(value / 2U)};
                    output[cursor++] = std::byte{static_cast<std::uint8_t>(255U - value)};
                    output[cursor++] = std::byte{0xffU};
                }
            }
        }
        extent = std::max(extent / 2U, 1U);
    }
    return true;
}

} // namespace gameengine::renderer::forward_plus
