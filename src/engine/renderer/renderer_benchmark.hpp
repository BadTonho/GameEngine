#pragma once

#include <array>
#include <span>

#include "engine/core/types.hpp"
#include "engine/math/math.hpp"

namespace gameengine::renderer::benchmark {

enum class LightingPath : core::u8 {
    forward = 0,
    forward_plus,
    clustered,
    deferred,
};

struct PointLight final {
    math::Vec3 position{};
    core::f32 radius = 0.0F;
    core::f32 intensity = 0.0F;
    core::f32 padding[3]{};
};

static_assert(sizeof(PointLight) == 32U);

struct BenchmarkCase final {
    LightingPath path = LightingPath::forward;
    core::u32 instance_count = 0;
    core::u32 light_count = 0;
};

struct ProcessMemorySnapshot final {
    core::u64 current_bytes = 0;
    core::u64 peak_bytes = 0;
    bool available = false;
};

constexpr std::array<core::u32, 3> instance_workloads = {1'000U, 10'000U, 100'000U};
constexpr std::array<core::u32, 3> light_workloads = {1U, 32U, 256U};
constexpr std::array<LightingPath, 4> lighting_paths = {
    LightingPath::forward,
    LightingPath::forward_plus,
    LightingPath::clustered,
    LightingPath::deferred,
};
constexpr core::u32 warmup_frames = 10U;
constexpr core::u32 measured_frames = 30U;
constexpr core::u32 frames_per_case = warmup_frames + measured_frames;
constexpr core::u32 case_count = static_cast<core::u32>(lighting_paths.size() *
                                                         instance_workloads.size() *
                                                         light_workloads.size());
constexpr core::u32 tile_width = 16U;
constexpr core::u32 tile_height = 16U;
constexpr core::u32 cluster_depth_slices = 24U;
constexpr core::u32 max_point_lights = 256U;

[[nodiscard]] constexpr const char* path_name(LightingPath path) noexcept
{
    switch (path) {
    case LightingPath::forward:
        return "forward";
    case LightingPath::forward_plus:
        return "forward_plus";
    case LightingPath::clustered:
        return "clustered";
    case LightingPath::deferred:
        return "deferred";
    }
    return "unknown";
}

[[nodiscard]] constexpr BenchmarkCase make_case(core::u32 index) noexcept
{
    const core::u32 light_index = index % static_cast<core::u32>(light_workloads.size());
    const core::u32 instance_index =
        (index / static_cast<core::u32>(light_workloads.size())) %
        static_cast<core::u32>(instance_workloads.size());
    const core::u32 path_index = index /
                                 (static_cast<core::u32>(light_workloads.size()) *
                                  static_cast<core::u32>(instance_workloads.size()));
    return {lighting_paths[path_index],
            instance_workloads[instance_index],
            light_workloads[light_index]};
}

[[nodiscard]] core::u32 tile_count(core::u32 width, core::u32 height) noexcept;
[[nodiscard]] core::u32 cluster_count(core::u32 width, core::u32 height) noexcept;
[[nodiscard]] core::u32 dispatch_work_items(LightingPath path,
                                              core::u32 width,
                                              core::u32 height,
                                              core::u32 instance_count,
                                              core::u32 light_count) noexcept;

[[nodiscard]] bool generate_point_lights(core::u32 count,
                                         std::span<PointLight> output) noexcept;

[[nodiscard]] ProcessMemorySnapshot read_process_memory() noexcept;

} // namespace gameengine::renderer::benchmark
