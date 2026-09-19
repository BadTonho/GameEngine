#include "engine/renderer/forward_plus.hpp"

#include <array>
#include <cassert>
#include <vector>

int main()
{
    using namespace gameengine::renderer;
    assert(forward_plus::tile_count(1280U, 720U) == 80U * 45U);
    std::array<benchmark::PointLight, 2> lights = {
        benchmark::PointLight{{0.0F, 0.0F, 0.0F}, 5.0F, 1.0F},
        benchmark::PointLight{{100.0F, 100.0F, 0.0F}, 2.0F, 1.0F},
    };
    std::vector<forward_plus::TileHeader> headers(forward_plus::tile_count(64U, 64U));
    std::vector<gameengine::core::u32> indices(
        forward_plus::maximum_index_count(64U, 64U, static_cast<gameengine::core::u32>(lights.size())));
    assert(forward_plus::build_tile_light_lists(64U, 64U, lights, headers, indices));
    bool found_light = false;
    for (const auto& header : headers) {
        found_light = found_light || header.count > 0U;
    }
    assert(found_light);
    const auto matrix = forward_plus::make_shadow_matrix({-1.0F, -2.0F, -1.0F});
    assert(gameengine::math::is_finite(matrix));
    const auto size = forward_plus::cubemap_rgba8_size(64U, 7U);
    std::vector<std::byte> cubemap(size);
    assert(forward_plus::generate_cubemap_rgba8(64U, 7U, cubemap));
    const auto first = cubemap;
    assert(first == cubemap);
    return 0;
}
