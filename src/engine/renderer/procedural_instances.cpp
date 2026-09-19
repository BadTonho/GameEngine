#include "engine/renderer/procedural_instances.hpp"

#include "engine/scene/bootstrap_cube.hpp"

#include <algorithm>
#include <cmath>

namespace gameengine::renderer::procedural {

namespace {

[[nodiscard]] core::u32 grid_side_for_count(core::u32 count) noexcept
{
    core::u32 side = 1U;
    while (static_cast<core::u64>(side) * side * side < count) {
        ++side;
    }
    return side;
}

[[nodiscard]] core::f32 maximum_basis_scale(const math::Mat4& matrix) noexcept
{
    core::f32 maximum = 0.0F;
    for (core::u32 column = 0; column < 3U; ++column) {
        const core::f32 x = matrix.at(0, column);
        const core::f32 y = matrix.at(1, column);
        const core::f32 z = matrix.at(2, column);
        maximum = std::max(maximum, std::sqrt(x * x + y * y + z * z));
    }
    return maximum;
}

} // namespace

core::f32 bootstrap_cube_bounding_radius() noexcept
{
    core::f32 radius_squared = 0.0F;
    for (const scene::TexturedVertex& vertex : scene::bootstrap_cube_vertices) {
        radius_squared = std::max(radius_squared, math::dot(vertex.position, vertex.position));
    }
    return std::sqrt(radius_squared);
}

bool generate_instances(core::u32 count,
                        const math::Mat4& prototype_transform,
                        std::span<ProceduralInstance> output) noexcept
{
    if (count > maximum_instance_count || output.size() < count ||
        !math::is_finite(prototype_transform)) {
        return false;
    }
    if (count == 0U) {
        return true;
    }

    const core::u32 side = grid_side_for_count(count);
    const core::f32 center = static_cast<core::f32>(side - 1U) * 0.5F;
    const core::f32 local_radius = bootstrap_cube_bounding_radius();
    const core::f32 scale = maximum_basis_scale(prototype_transform);
    if (!std::isfinite(local_radius) || !std::isfinite(scale) || scale <= 0.0F) {
        return false;
    }

    for (core::u32 index = 0; index < count; ++index) {
        const core::u32 x = index % side;
        const core::u32 y = (index / side) % side;
        const core::u32 z = index / (side * side);
        const math::Vec3 position{
            (static_cast<core::f32>(x) - center) * instance_grid_spacing,
            (static_cast<core::f32>(y) - center) * instance_grid_spacing,
            (static_cast<core::f32>(z) - center) * instance_grid_spacing,
        };
        const math::Mat4 model = math::multiply(math::translation(position), prototype_transform);
        output[index] = {
            .data = {.model = model},
            .center = math::transform_point(model, {}),
            .radius = local_radius * scale,
        };
        if (!math::is_finite(model) || !math::is_finite(output[index].center) ||
            !std::isfinite(output[index].radius) || output[index].radius <= 0.0F) {
            return false;
        }
    }
    return true;
}

bool cull_instances(std::span<const ProceduralInstance> instances,
                    const math::Frustum& frustum,
                    std::span<InstanceData> visible_output,
                    core::u32& visible_count) noexcept
{
    visible_count = 0U;
    if (instances.size() > visible_output.size() ||
        instances.size() > maximum_instance_count) {
        return false;
    }
    for (const ProceduralInstance& instance : instances) {
        if (!math::sphere_visible(frustum, instance.center, instance.radius)) {
            continue;
        }
        visible_output[visible_count++] = instance.data;
    }
    return true;
}

} // namespace gameengine::renderer::procedural
