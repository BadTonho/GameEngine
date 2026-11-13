#pragma once

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/render_graph/render_graph.hpp"

namespace gameengine::renderer::advanced {

struct ScaledExtent final {
    core::u32 width = 1U;
    core::u32 height = 1U;
    float clamped_scale = 1.0F;
};

[[nodiscard]] ScaledExtent compute_upscale_input_extent(core::u32 output_width,
                                                        core::u32 output_height,
                                                        float render_scale) noexcept;

[[nodiscard]] core::Status register_upscale_pass(render_graph::RenderGraph& graph,
                                                 render_graph::ResourceHandle input_color,
                                                 render_graph::ResourceHandle output_color,
                                                 render_graph::PassHandle& out_pass) noexcept;

} // namespace gameengine::renderer::advanced
