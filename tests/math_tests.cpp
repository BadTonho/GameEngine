#include "engine/math/math.hpp"
#include "engine/scene/bootstrap_cube.hpp"
#include "engine/scene/bootstrap_material.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {

[[nodiscard]] bool near(float left, float right, float epsilon = 0.0001F) noexcept
{
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main()
{
    using namespace gameengine::math;

    const Mat4 identity = Mat4::identity();
    const Mat4 multiplied = multiply(identity, identity);
    for (std::size_t index = 0; index < identity.values.size(); ++index) {
        if (!near(multiplied.values[index], identity.values[index])) {
            return 1;
        }
    }

    const Mat4 view = look_at_rh({0.0F, 0.0F, 4.0F}, {0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
    if (!near(view.at(0, 0), 1.0F) || !near(view.at(1, 1), 1.0F) ||
        !near(view.at(2, 2), 1.0F) || !near(view.at(2, 3), -4.0F)) {
        return 2;
    }

    const Mat4 projection = perspective_rh_zo(1.04719755F, 16.0F / 9.0F, 0.1F, 100.0F);
    if (!near(projection.at(1, 1), -projection.at(0, 0) * (16.0F / 9.0F), 0.001F) ||
        projection.at(2, 3) >= 0.0F || !near(projection.at(3, 2), -1.0F)) {
        return 3;
    }

    if (gameengine::scene::bootstrap_cube_vertices.size() != 24U ||
        gameengine::scene::bootstrap_cube_indices.size() != 36U) {
        return 4;
    }
    for (const std::uint16_t index : gameengine::scene::bootstrap_cube_indices) {
        if (index >= gameengine::scene::bootstrap_cube_vertices.size()) {
            return 5;
        }
    }
    for (const auto& vertex : gameengine::scene::bootstrap_cube_vertices) {
        if (vertex.position.x < -1.0F || vertex.position.x > 1.0F ||
            vertex.position.y < -1.0F || vertex.position.y > 1.0F ||
            vertex.position.z < -1.0F || vertex.position.z > 1.0F) {
            return 6;
        }
        const float normal_length = std::sqrt(dot(vertex.normal, vertex.normal));
        if (!near(normal_length, 1.0F) || vertex.uv.x < 0.0F || vertex.uv.x > 1.0F ||
            vertex.uv.y < 0.0F || vertex.uv.y > 1.0F) {
            return 7;
        }
    }

    static_assert(sizeof(gameengine::math::Vec2) == sizeof(float) * 2U);
    static_assert(sizeof(gameengine::scene::TexturedVertex) == sizeof(float) * 8U);

    static_assert(gameengine::scene::bootstrap_checkerboard.size() ==
                  gameengine::scene::bootstrap_texture_byte_count);
    static_assert(gameengine::scene::bootstrap_checkerboard[0] !=
                  gameengine::scene::bootstrap_checkerboard[
                      8U * gameengine::scene::bootstrap_texture_channels]);
    static_assert(gameengine::scene::BootstrapMaterialConstants{}.base_color_factor[0] == 1.0F);

    return 0;
}
