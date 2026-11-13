#include "engine/renderer/advanced_graphics/temporal_aa.hpp"

#include <algorithm>
#include <array>

namespace gameengine::renderer::advanced {

float halton_sequence(core::u32 index, core::u32 base) noexcept
{
    if (base < 2U) {
        return 0.0F;
    }
    float f = 1.0F;
    float result = 0.0F;
    core::u32 i = index;
    while (i > 0U) {
        f /= static_cast<float>(base);
        result += f * static_cast<float>(i % base);
        i /= base;
    }
    return result;
}

SubpixelJitter compute_taa_jitter(core::u64 frame_index, core::u32 sample_count) noexcept
{
    const core::u32 count = std::max(sample_count, 1U);
    const core::u32 idx = static_cast<core::u32>((frame_index % count) + 1U);
    return SubpixelJitter{
        .x = halton_sequence(idx, 2U) - 0.5F,
        .y = halton_sequence(idx, 3U) - 0.5F,
    };
}

core::Status register_taa_pass(render_graph::RenderGraph& graph,
                               render_graph::ResourceHandle scene_color,
                               render_graph::ResourceHandle history_color,
                               render_graph::ResourceHandle resolved_color,
                               render_graph::PassHandle& out_pass) noexcept
{
    const std::array<render_graph::ResourceHandle, 2> reads = {scene_color, history_color};
    const std::array<render_graph::ResourceHandle, 1> writes = {resolved_color};
    render_graph::PassDescription desc{};
    desc.name = "taa_resolve";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
}

} // namespace gameengine::renderer::advanced
