#pragma once

#include <array>
#include <cstddef>

#include "engine/core/types.hpp"

namespace gameengine::scene {

constexpr core::u32 bootstrap_texture_width = 64;
constexpr core::u32 bootstrap_texture_height = 64;
constexpr core::u32 bootstrap_texture_channels = 4;
constexpr core::usize bootstrap_texture_byte_count =
    static_cast<core::usize>(bootstrap_texture_width) * bootstrap_texture_height *
    bootstrap_texture_channels;

inline constexpr std::array<std::byte, bootstrap_texture_byte_count> bootstrap_checkerboard = [] {
    std::array<std::byte, bootstrap_texture_byte_count> pixels{};
    for (core::u32 y = 0; y < bootstrap_texture_height; ++y) {
        for (core::u32 x = 0; x < bootstrap_texture_width; ++x) {
            const bool bright = ((x / 8U) + (y / 8U)) % 2U == 0U;
            const std::byte value = bright ? std::byte{0xe6} : std::byte{0x28};
            const core::usize offset =
                (static_cast<core::usize>(y) * bootstrap_texture_width + x) *
                bootstrap_texture_channels;
            pixels[offset + 0U] = value;
            pixels[offset + 1U] = bright ? std::byte{0xa8} : std::byte{0x36};
            pixels[offset + 2U] = bright ? std::byte{0x42} : std::byte{0x18};
            pixels[offset + 3U] = std::byte{0xff};
        }
    }
    return pixels;
}();

struct alignas(16) BootstrapMaterialConstants final {
    std::array<core::f32, 4> base_color_factor = {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<core::f32, 4> light_direction_intensity = {-0.45F, -0.8F, -0.35F, 4.0F};
    std::array<core::f32, 4> camera_position_exposure = {2.5F, 2.0F, 4.0F, 1.0F};
    std::array<core::f32, 4> material_parameters = {0.05F, 0.42F, 0.03F, 0.0F};
    std::array<core::f32, 16> shadow_view_projection = {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
};

static_assert(sizeof(BootstrapMaterialConstants) == 128U);
static_assert(alignof(BootstrapMaterialConstants) == 16U);

} // namespace gameengine::scene
