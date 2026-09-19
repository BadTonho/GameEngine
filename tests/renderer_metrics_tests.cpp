#include "engine/renderer/renderer_metrics.hpp"

#include <limits>

namespace {

using gameengine::core::u64;
using gameengine::renderer::metrics::FrameTimingReport;
using gameengine::renderer::metrics::TimingAccumulator;
using gameengine::renderer::metrics::timestamp_delta_to_nanoseconds;

bool test_timestamp_conversion() noexcept
{
    if (timestamp_delta_to_nanoseconds(10U, 14U, 2.5, 64U) != 10U ||
        timestamp_delta_to_nanoseconds(std::numeric_limits<u64>::max() - 1U,
                                       1U,
                                       1.0,
                                       64U) != 3U ||
        timestamp_delta_to_nanoseconds(0U, 8U, 2.0, 4U) != 16U ||
        timestamp_delta_to_nanoseconds(0U, 8U, 0.0, 64U) != 0U) {
        return false;
    }
    return timestamp_delta_to_nanoseconds(0U, 8U, 2.0, 4U) == 16U &&
           timestamp_delta_to_nanoseconds(0U, 15U, 1.0, 4U) == 15U;
}

bool test_empty_and_populated_reports() noexcept
{
    TimingAccumulator accumulator;
    accumulator.reset();
    FrameTimingReport empty;
    empty.reset(false);
    accumulator.record(empty);
    if (accumulator.frame_count != 1U || accumulator.total_draw_calls != 0U ||
        accumulator.gpu_timestamps_available) {
        return false;
    }

    FrameTimingReport report;
    report.reset(true);
    report.total_instances = 100U;
    report.visible_instances = 12U;
    report.culled_instances = 88U;
    report.visibility_cpu_nanoseconds = 7U;
    report.gpu_culling_cpu_nanoseconds = 11U;
    report.instance_buffer_bytes = 128U;
    report.gpu_source_buffer_bytes = 256U;
    report.gpu_visible_buffer_bytes = 512U;
    report.gpu_indirect_buffer_bytes = 64U;
    report.benchmark_active = true;
    report.benchmark_path = gameengine::renderer::benchmark::LightingPath::clustered;
    report.benchmark_light_count = 32U;
    report.visibility_mode = gameengine::renderer::gpu_culling::VisibilityMode::gpu;
    report.gpu_culling_available = true;
    report.gpu_culling_active = true;
    report.gpu_culling_fallback = false;
    report.add_pass("clustered_light_cull", 5U, 0U, true, 1U);
    report.add_pass("forward_opaque", 10U, 1U, true);
    report.passes[0].gpu_nanoseconds = 20U;
    accumulator.record(report);
    const auto& pass = accumulator.passes[0];
    return accumulator.frame_count == 2U && accumulator.total_draw_calls == 1U &&
           accumulator.total_dispatch_calls == 1U && accumulator.benchmark_active &&
           accumulator.benchmark_path == gameengine::renderer::benchmark::LightingPath::clustered &&
           accumulator.benchmark_light_count == 32U &&
           accumulator.total_instances == 100U && accumulator.visible_instances == 12U &&
           accumulator.culled_instances == 88U &&
           accumulator.visibility_cpu_total_nanoseconds == 7U &&
           accumulator.visibility_cpu_min_nanoseconds == 0U &&
           accumulator.visibility_cpu_max_nanoseconds == 7U &&
           accumulator.gpu_culling_cpu_total_nanoseconds == 11U &&
           accumulator.gpu_culling_cpu_min_nanoseconds == 0U &&
           accumulator.gpu_culling_cpu_max_nanoseconds == 11U &&
           accumulator.instance_buffer_bytes == 128U &&
           accumulator.gpu_source_buffer_bytes == 256U &&
           accumulator.gpu_visible_buffer_bytes == 512U &&
           accumulator.gpu_indirect_buffer_bytes == 64U &&
           accumulator.visibility_mode == gameengine::renderer::gpu_culling::VisibilityMode::gpu &&
           accumulator.gpu_culling_available && accumulator.gpu_culling_active &&
           !accumulator.gpu_culling_fallback &&
           accumulator.gpu_timestamps_available && pass.name == "clustered_light_cull" &&
           pass.sample_count == 1U && pass.cpu_total_nanoseconds == 5U &&
           pass.cpu_min_nanoseconds == 5U && pass.cpu_max_nanoseconds == 5U &&
           pass.dispatch_calls == 1U && pass.gpu_sample_count == 1U &&
           pass.gpu_total_nanoseconds == 20U;
}

} // namespace

int main()
{
    return test_timestamp_conversion() && test_empty_and_populated_reports() ? 0 : 1;
}
