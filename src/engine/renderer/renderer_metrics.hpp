#pragma once

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/gpu_culling.hpp"

namespace gameengine::rhi {
class Renderer;
}

namespace gameengine::renderer::metrics {

constexpr core::u32 max_timed_passes = 8U;

struct PassTiming final {
    std::string_view name{};
    core::u64 cpu_nanoseconds = 0;
    core::u64 gpu_nanoseconds = 0;
    core::u32 draw_calls = 0;
    bool gpu_time_valid = false;
};

struct FrameTimingReport final {
    std::array<PassTiming, max_timed_passes> passes{};
    core::u32 pass_count = 0;
    core::u32 draw_calls = 0;
    core::u32 total_instances = 0;
    core::u32 visible_instances = 0;
    core::u32 culled_instances = 0;
    core::u64 visibility_cpu_nanoseconds = 0;
    core::u64 gpu_culling_cpu_nanoseconds = 0;
    core::u64 instance_buffer_bytes = 0;
    core::u64 gpu_source_buffer_bytes = 0;
    core::u64 gpu_visible_buffer_bytes = 0;
    core::u64 gpu_indirect_buffer_bytes = 0;
    gpu_culling::VisibilityMode visibility_mode = gpu_culling::VisibilityMode::cpu;
    bool gpu_culling_available = false;
    bool gpu_culling_active = false;
    bool gpu_culling_fallback = false;
    bool gpu_timestamps_available = false;

    void reset(bool gpu_available,
               gpu_culling::VisibilityMode mode = gpu_culling::VisibilityMode::cpu,
               bool gpu_available_for_culling = false,
               bool gpu_active = false,
               bool gpu_fallback = false) noexcept
    {
        passes = {};
        pass_count = 0;
        draw_calls = 0;
        total_instances = 0;
        visible_instances = 0;
        culled_instances = 0;
        visibility_cpu_nanoseconds = 0;
        gpu_culling_cpu_nanoseconds = 0;
        instance_buffer_bytes = 0;
        gpu_source_buffer_bytes = 0;
        gpu_visible_buffer_bytes = 0;
        gpu_indirect_buffer_bytes = 0;
        visibility_mode = mode;
        gpu_culling_available = gpu_available_for_culling;
        gpu_culling_active = gpu_active;
        gpu_culling_fallback = gpu_fallback;
        gpu_timestamps_available = gpu_available;
    }

    void add_pass(std::string_view pass_name,
                  core::u64 cpu_nanoseconds_value,
                  core::u32 pass_draw_calls,
                  bool gpu_available) noexcept
    {
        if (pass_count >= max_timed_passes) {
            return;
        }
        PassTiming& timing = passes[pass_count++];
        timing.name = pass_name;
        timing.cpu_nanoseconds = cpu_nanoseconds_value;
        timing.draw_calls = pass_draw_calls;
        timing.gpu_time_valid = gpu_available;
        draw_calls += pass_draw_calls;
    }
};

struct PassTimingAggregate final {
    std::string_view name{};
    core::u64 sample_count = 0;
    core::u64 cpu_total_nanoseconds = 0;
    core::u64 cpu_min_nanoseconds = std::numeric_limits<core::u64>::max();
    core::u64 cpu_max_nanoseconds = 0;
    core::u64 gpu_total_nanoseconds = 0;
    core::u64 gpu_min_nanoseconds = std::numeric_limits<core::u64>::max();
    core::u64 gpu_max_nanoseconds = 0;
    core::u64 draw_calls = 0;
    core::u64 gpu_sample_count = 0;
};

struct TimingAccumulator final {
    std::array<PassTimingAggregate, max_timed_passes> passes{};
    core::u64 frame_count = 0;
    core::u64 total_draw_calls = 0;
    core::u64 total_instances = 0;
    core::u64 visible_instances = 0;
    core::u64 culled_instances = 0;
    core::u64 visibility_cpu_total_nanoseconds = 0;
    core::u64 visibility_cpu_min_nanoseconds = std::numeric_limits<core::u64>::max();
    core::u64 visibility_cpu_max_nanoseconds = 0;
    core::u64 gpu_culling_cpu_total_nanoseconds = 0;
    core::u64 gpu_culling_cpu_min_nanoseconds = std::numeric_limits<core::u64>::max();
    core::u64 gpu_culling_cpu_max_nanoseconds = 0;
    core::u64 instance_buffer_bytes = 0;
    core::u64 gpu_source_buffer_bytes = 0;
    core::u64 gpu_visible_buffer_bytes = 0;
    core::u64 gpu_indirect_buffer_bytes = 0;
    gpu_culling::VisibilityMode visibility_mode = gpu_culling::VisibilityMode::cpu;
    bool gpu_culling_available = false;
    bool gpu_culling_active = false;
    bool gpu_culling_fallback = false;
    bool gpu_timestamps_available = false;

    void reset() noexcept
    {
        passes = {};
        frame_count = 0;
        total_draw_calls = 0;
        total_instances = 0;
        visible_instances = 0;
        culled_instances = 0;
        visibility_cpu_total_nanoseconds = 0;
        visibility_cpu_min_nanoseconds = std::numeric_limits<core::u64>::max();
        visibility_cpu_max_nanoseconds = 0;
        gpu_culling_cpu_total_nanoseconds = 0;
        gpu_culling_cpu_min_nanoseconds = std::numeric_limits<core::u64>::max();
        gpu_culling_cpu_max_nanoseconds = 0;
        instance_buffer_bytes = 0;
        gpu_source_buffer_bytes = 0;
        gpu_visible_buffer_bytes = 0;
        gpu_indirect_buffer_bytes = 0;
        visibility_mode = gpu_culling::VisibilityMode::cpu;
        gpu_culling_available = false;
        gpu_culling_active = false;
        gpu_culling_fallback = false;
        gpu_timestamps_available = false;
    }

    void record(const FrameTimingReport& report) noexcept
    {
        ++frame_count;
        total_draw_calls += report.draw_calls;
        total_instances += report.total_instances;
        visible_instances += report.visible_instances;
        culled_instances += report.culled_instances;
        visibility_cpu_total_nanoseconds += report.visibility_cpu_nanoseconds;
        visibility_cpu_min_nanoseconds =
            std::min(visibility_cpu_min_nanoseconds, report.visibility_cpu_nanoseconds);
        visibility_cpu_max_nanoseconds =
            std::max(visibility_cpu_max_nanoseconds, report.visibility_cpu_nanoseconds);
        gpu_culling_cpu_total_nanoseconds += report.gpu_culling_cpu_nanoseconds;
        gpu_culling_cpu_min_nanoseconds =
            std::min(gpu_culling_cpu_min_nanoseconds, report.gpu_culling_cpu_nanoseconds);
        gpu_culling_cpu_max_nanoseconds =
            std::max(gpu_culling_cpu_max_nanoseconds, report.gpu_culling_cpu_nanoseconds);
        instance_buffer_bytes = report.instance_buffer_bytes;
        gpu_source_buffer_bytes = report.gpu_source_buffer_bytes;
        gpu_visible_buffer_bytes = report.gpu_visible_buffer_bytes;
        gpu_indirect_buffer_bytes = report.gpu_indirect_buffer_bytes;
        visibility_mode = report.visibility_mode;
        gpu_culling_available = report.gpu_culling_available;
        gpu_culling_active = report.gpu_culling_active;
        gpu_culling_fallback = report.gpu_culling_fallback;
        gpu_timestamps_available = gpu_timestamps_available ||
                                    report.gpu_timestamps_available;
        for (core::u32 index = 0; index < report.pass_count; ++index) {
            const PassTiming& timing = report.passes[index];
            PassTimingAggregate* aggregate = nullptr;
            for (PassTimingAggregate& candidate : passes) {
                if (candidate.name == timing.name) {
                    aggregate = &candidate;
                    break;
                }
                if (candidate.name.empty() && aggregate == nullptr) {
                    aggregate = &candidate;
                }
            }
            if (aggregate == nullptr) {
                continue;
            }
            aggregate->name = timing.name;
            ++aggregate->sample_count;
            aggregate->cpu_total_nanoseconds += timing.cpu_nanoseconds;
            aggregate->cpu_min_nanoseconds =
                std::min(aggregate->cpu_min_nanoseconds, timing.cpu_nanoseconds);
            aggregate->cpu_max_nanoseconds =
                std::max(aggregate->cpu_max_nanoseconds, timing.cpu_nanoseconds);
            aggregate->draw_calls += timing.draw_calls;
            if (timing.gpu_time_valid) {
                ++aggregate->gpu_sample_count;
                aggregate->gpu_total_nanoseconds += timing.gpu_nanoseconds;
                aggregate->gpu_min_nanoseconds =
                    std::min(aggregate->gpu_min_nanoseconds, timing.gpu_nanoseconds);
                aggregate->gpu_max_nanoseconds =
                    std::max(aggregate->gpu_max_nanoseconds, timing.gpu_nanoseconds);
            }
        }
    }
};

[[nodiscard]] inline core::u64 timestamp_delta_to_nanoseconds(core::u64 start,
                                                               core::u64 end,
                                                               core::f64 timestamp_period,
                                                               core::u32 valid_bits) noexcept
{
    if (timestamp_period <= 0.0 || valid_bits == 0U) {
        return 0;
    }

    core::u64 delta = end - start;
    if (valid_bits < 64U) {
        const core::u64 mask = (core::u64{1} << valid_bits) - 1U;
        delta &= mask;
    }

    const long double nanoseconds = static_cast<long double>(delta) *
                                    static_cast<long double>(timestamp_period);
    const long double maximum = static_cast<long double>(
        std::numeric_limits<core::u64>::max());
    if (nanoseconds >= maximum) {
        return std::numeric_limits<core::u64>::max();
    }
    return static_cast<core::u64>(nanoseconds);
}

} // namespace gameengine::renderer::metrics

namespace gameengine::renderer::diagnostics {

void begin_metrics(const gameengine::rhi::Renderer& renderer) noexcept;
[[nodiscard]] core::Status set_visibility_mode(
    const gameengine::rhi::Renderer& renderer,
    gpu_culling::VisibilityMode mode) noexcept;
[[nodiscard]] core::Status set_procedural_workload(const gameengine::rhi::Renderer& renderer,
                                                   core::u32 instance_count) noexcept;
void print_metrics(const gameengine::rhi::Renderer& renderer) noexcept;

} // namespace gameengine::renderer::diagnostics
