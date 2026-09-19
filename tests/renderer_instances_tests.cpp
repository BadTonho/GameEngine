#include "engine/math/math.hpp"
#include "engine/renderer/procedural_instances.hpp"
#include "engine/scene/bootstrap_cube.hpp"

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace {

[[nodiscard]] bool near(float left, float right, float epsilon = 0.0001F) noexcept
{
    return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] bool same_matrix(const gameengine::math::Mat4& left,
                               const gameengine::math::Mat4& right) noexcept
{
    for (std::size_t index = 0; index < left.values.size(); ++index) {
        if (!near(left.values[index], right.values[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    using namespace gameengine;

    static_assert(sizeof(renderer::procedural::InstanceData) == sizeof(float) * 16U);
    static_assert(offsetof(renderer::procedural::InstanceData, model) == 0U);
    static_assert(offsetof(scene::TexturedVertex, position) == 0U);
    static_assert(offsetof(scene::TexturedVertex, normal) == sizeof(float) * 3U);
    static_assert(offsetof(scene::TexturedVertex, uv) == sizeof(float) * 6U);
    static_assert(sizeof(scene::TexturedVertex) == sizeof(float) * 8U);

    std::vector<renderer::procedural::ProceduralInstance> first(
        renderer::procedural::maximum_instance_count);
    std::vector<renderer::procedural::ProceduralInstance> second(
        renderer::procedural::maximum_instance_count);
    const math::Mat4 prototype = math::multiply(math::rotation_y(0.65F), math::rotation_x(-0.4F));
    if (!renderer::procedural::generate_instances(
            1'000U, prototype, std::span<renderer::procedural::ProceduralInstance>{first.data(), 1'000U}) ||
        !renderer::procedural::generate_instances(
            1'000U, prototype, std::span<renderer::procedural::ProceduralInstance>{second.data(), 1'000U})) {
        return 1;
    }
    for (std::size_t index = 0; index < 1'000U; ++index) {
        if (!same_matrix(first[index].data.model, second[index].data.model) ||
            !near(first[index].center.x, second[index].center.x) ||
            !near(first[index].center.y, second[index].center.y) ||
            !near(first[index].center.z, second[index].center.z) ||
            !near(first[index].radius, second[index].radius) ||
            first[index].radius <= 0.0F) {
            return 2;
        }
    }

    if (!renderer::procedural::generate_instances(
            renderer::procedural::maximum_instance_count,
            prototype,
            std::span<renderer::procedural::ProceduralInstance>{
                first.data(), renderer::procedural::maximum_instance_count}) ||
        first.back().center.z <= first.front().center.z) {
        return 3;
    }

    if (!renderer::procedural::generate_instances(
            1'000U, prototype, std::span<renderer::procedural::ProceduralInstance>{first.data(), 1'000U})) {
        return 4;
    }

    math::Frustum identity_frustum{};
    if (!math::extract_frustum_rh_zo(math::Mat4::identity(), identity_frustum) ||
        !math::sphere_visible(identity_frustum, {0.0F, 0.0F, 0.5F}, 0.1F) ||
        math::sphere_visible(identity_frustum, {2.0F, 0.0F, 0.5F}, 0.1F)) {
        return 5;
    }

    const math::Mat4 view = math::look_at_rh(
        {2.5F, 2.0F, 4.0F}, {0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
    const math::Mat4 projection = math::perspective_rh_zo(
        1.04719755F, 16.0F / 9.0F, 0.1F, 100.0F);
    math::Frustum camera_frustum{};
    if (!math::extract_frustum_rh_zo(math::multiply(projection, view), camera_frustum)) {
        return 6;
    }

    std::vector<renderer::procedural::InstanceData> visible(1'000U);
    core::u32 visible_count = 0U;
    if (!renderer::procedural::cull_instances(
            std::span<const renderer::procedural::ProceduralInstance>{first.data(), 1'000U},
            camera_frustum,
            visible,
            visible_count) ||
        visible_count == 0U || visible_count > 1'000U) {
        return 7;
    }

    math::Frustum invalid_frustum{};
    core::u32 fallback_count = 0U;
    if (!renderer::procedural::cull_instances(
            std::span<const renderer::procedural::ProceduralInstance>{first.data(), 1'000U},
            invalid_frustum,
            visible,
            fallback_count) ||
        fallback_count != 1'000U) {
        return 8;
    }

    return 0;
}
