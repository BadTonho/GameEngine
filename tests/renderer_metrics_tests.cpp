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
    report.instance_buffer_bytes = 128U;
    report.add_pass("forward_opaque", 10U, 1U, true);
    report.passes[0].gpu_nanoseconds = 20U;
    accumulator.record(report);
    const auto& pass = accumulator.passes[0];
    return accumulator.frame_count == 2U && accumulator.total_draw_calls == 1U &&
           accumulator.total_instances == 100U && accumulator.visible_instances == 12U &&
           accumulator.culled_instances == 88U &&
           accumulator.visibility_cpu_total_nanoseconds == 7U &&
           accumulator.visibility_cpu_min_nanoseconds == 0U &&
           accumulator.visibility_cpu_max_nanoseconds == 7U &&
           accumulator.instance_buffer_bytes == 128U &&
           accumulator.gpu_timestamps_available && pass.name == "forward_opaque" &&
           pass.sample_count == 1U && pass.cpu_total_nanoseconds == 10U &&
           pass.cpu_min_nanoseconds == 10U && pass.cpu_max_nanoseconds == 10U &&
           pass.gpu_sample_count == 1U && pass.gpu_total_nanoseconds == 20U;
}

} // namespace

int main()
{
    return test_timestamp_conversion() && test_empty_and_populated_reports() ? 0 : 1;
}
