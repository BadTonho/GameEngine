#pragma once

#include <array>

#include "engine/core/types.hpp"
#include "engine/math/math.hpp"
#include "engine/renderer/procedural_instances.hpp"

namespace gameengine::renderer::gpu_culling {

constexpr core::u32 workgroup_size = 64U;
constexpr core::u32 indirect_index_count = 36U;

enum class VisibilityMode : core::u8 {
    cpu = 0,
    gpu,
};

[[nodiscard]] constexpr VisibilityMode resolve_visibility_mode(
    VisibilityMode requested,
    bool available) noexcept
{
    return requested == VisibilityMode::gpu && available ? VisibilityMode::gpu
                                                         : VisibilityMode::cpu;
}

struct GpuCullInstance final {
    math::Mat4 model{};
    math::Vec3 center{};
    core::f32 radius = 0.0F;
};

static_assert(sizeof(GpuCullInstance) == 80U);
static_assert(alignof(GpuCullInstance) == alignof(core::f32));
static_assert(sizeof(GpuCullInstance) == sizeof(procedural::ProceduralInstance));

struct GpuCullPlane final {
    core::f32 x = 0.0F;
    core::f32 y = 0.0F;
    core::f32 z = 0.0F;
    core::f32 distance = 0.0F;
};

struct GpuCullPushConstants final {
    std::array<GpuCullPlane, 6> planes{};
    core::u32 instance_count = 0;
    std::array<core::u32, 3> padding{};
};

static_assert(sizeof(GpuCullPushConstants) == 112U);
static_assert(sizeof(GpuCullPushConstants) % 16U == 0U);

struct IndirectCommand final {
    core::u32 index_count = indirect_index_count;
    core::u32 instance_count = 0;
    core::u32 first_index = 0;
    core::u32 vertex_offset = 0;
    core::u32 first_instance = 0;
};

static_assert(sizeof(IndirectCommand) == 20U);

[[nodiscard]] constexpr core::u32 dispatch_group_count(core::u32 instance_count) noexcept
{
    return (instance_count + workgroup_size - 1U) / workgroup_size;
}

[[nodiscard]] inline GpuCullInstance to_gpu_instance(
    const procedural::ProceduralInstance& instance) noexcept
{
    return {
        .model = instance.data.model,
        .center = instance.center,
        .radius = instance.radius,
    };
}

[[nodiscard]] inline GpuCullPushConstants make_push_constants(
    const math::Frustum& frustum,
    core::u32 instance_count) noexcept
{
    GpuCullPushConstants result{};
    result.instance_count = instance_count;
    if (!frustum.valid) {
        return result;
    }
    for (core::u32 index = 0; index < result.planes.size(); ++index) {
        result.planes[index] = {
            frustum.planes[index].normal.x,
            frustum.planes[index].normal.y,
            frustum.planes[index].normal.z,
            frustum.planes[index].distance,
        };
    }
    return result;
}

[[nodiscard]] constexpr IndirectCommand initial_indirect_command() noexcept
{
    return {};
}

} // namespace gameengine::renderer::gpu_culling
