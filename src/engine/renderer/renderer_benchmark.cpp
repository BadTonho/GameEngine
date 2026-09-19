#include "engine/renderer/renderer_benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace gameengine::renderer::benchmark {

core::u32 tile_count(core::u32 width, core::u32 height) noexcept
{
    if (width == 0U || height == 0U) {
        return 0U;
    }
    const core::u32 columns = (width + tile_width - 1U) / tile_width;
    const core::u32 rows = (height + tile_height - 1U) / tile_height;
    return columns * rows;
}

core::u32 cluster_count(core::u32 width, core::u32 height) noexcept
{
    return tile_count(width, height) * cluster_depth_slices;
}

core::u32 dispatch_work_items(LightingPath path,
                              core::u32 width,
                              core::u32 height,
                              core::u32 instance_count,
                              core::u32 light_count) noexcept
{
    const core::u32 light_batches = std::max(1U, (light_count + 31U) / 32U);
    switch (path) {
    case LightingPath::forward:
        return 0U;
    case LightingPath::forward_plus:
        return tile_count(width, height) * light_batches;
    case LightingPath::clustered:
        return cluster_count(width, height) * light_batches;
    case LightingPath::deferred:
        return std::max(instance_count, tile_count(width, height) * light_batches);
    }
    return 0U;
}

bool generate_point_lights(core::u32 count, std::span<PointLight> output) noexcept
{
    if (count == 0U || count > max_point_lights || output.size() < count) {
        return false;
    }
    constexpr core::f32 golden_angle = 2.39996322972865332F;
    for (core::u32 index = 0; index < count; ++index) {
        const core::f32 normalized = (static_cast<core::f32>(index) + 0.5F) /
                                     static_cast<core::f32>(count);
        const core::f32 radius = 2.0F + 8.0F * normalized;
        const core::f32 angle = golden_angle * static_cast<core::f32>(index);
        output[index] = {
            .position = {std::cos(angle) * radius,
                         -1.5F + 3.0F * normalized,
                         std::sin(angle) * radius},
            .radius = 3.0F + 5.0F * normalized,
            .intensity = 1.0F + 2.0F * (1.0F - normalized),
            .padding = {0.0F, 0.0F, 0.0F},
        };
    }
    return true;
}

ProcessMemorySnapshot read_process_memory() noexcept
{
    ProcessMemorySnapshot result{};
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != FALSE) {
        result.current_bytes = static_cast<core::u64>(counters.WorkingSetSize);
        result.peak_bytes = static_cast<core::u64>(counters.PeakWorkingSetSize);
        result.available = true;
    }
#elif defined(__linux__)
    std::FILE* file = std::fopen("/proc/self/status", "r");
    if (file == nullptr) {
        return result;
    }
    char line[256]{};
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        unsigned long long value = 0;
        if (std::sscanf(line, "VmRSS: %llu kB", &value) == 1) {
            result.current_bytes = static_cast<core::u64>(value) * 1024U;
        } else if (std::sscanf(line, "VmHWM: %llu kB", &value) == 1) {
            result.peak_bytes = static_cast<core::u64>(value) * 1024U;
        }
    }
    std::fclose(file);
    result.available = result.current_bytes != 0U || result.peak_bytes != 0U;
#endif
    return result;
}

} // namespace gameengine::renderer::benchmark
