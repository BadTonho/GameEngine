#pragma once

#include <span>

#include "engine/core/types.hpp"
#include "engine/math/math.hpp"

namespace gameengine::renderer::procedural {

constexpr core::u32 default_instance_count = 1'000U;
constexpr core::u32 maximum_instance_count = 100'000U;
constexpr core::f32 instance_grid_spacing = 3.0F;

struct InstanceData final {
    math::Mat4 model{};
};

static_assert(sizeof(InstanceData) == sizeof(core::f32) * 16U);

struct ProceduralInstance final {
    InstanceData data{};
    math::Vec3 center{};
    core::f32 radius = 0.0F;
};

[[nodiscard]] core::f32 bootstrap_cube_bounding_radius() noexcept;

[[nodiscard]] bool generate_instances(core::u32 count,
                                      const math::Mat4& prototype_transform,
                                      std::span<ProceduralInstance> output) noexcept;

[[nodiscard]] bool cull_instances(std::span<const ProceduralInstance> instances,
                                  const math::Frustum& frustum,
                                  std::span<InstanceData> visible_output,
                                  core::u32& visible_count) noexcept;

} // namespace gameengine::renderer::procedural
