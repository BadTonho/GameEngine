#include "engine/renderer/advanced_graphics/upscaling.hpp"

#include <algorithm>
#include <array>

namespace gameengine::renderer::advanced {

ScaledExtent compute_upscale_input_extent(core::u32 output_width,
                                          core::u32 output_height,
                                          float render_scale) noexcept
{
    const float clamped = std::clamp(render_scale, 0.5F, 1.0F);
    const core::u32 w = std::max(
        static_cast<core::u32>(static_cast<float>(std::max(output_width, 1U)) * clamped), 1U);
    const core::u32 h = std::max(
        static_cast<core::u32>(static_cast<float>(std::max(output_height, 1U)) * clamped), 1U);
    return ScaledExtent{
        .width = w,
        .height = h,
        .clamped_scale = clamped,
    };
}

core::Status register_upscale_pass(render_graph::RenderGraph& graph,
                                   render_graph::ResourceHandle input_color,
                                   render_graph::ResourceHandle output_color,
                                   render_graph::PassHandle& out_pass) noexcept
{
    const std::array<render_graph::ResourceHandle, 1> reads = {input_color};
    const std::array<render_graph::ResourceHandle, 1> writes = {output_color};
    render_graph::PassDescription desc{};
    desc.name = "upscale";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
}

} // namespace gameengine::renderer::advanced
