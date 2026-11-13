#pragma once

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/render_graph/render_graph.hpp"

namespace gameengine::renderer::advanced {

struct SubpixelJitter final {
    float x = 0.0F;
    float y = 0.0F;
};

[[nodiscard]] float halton_sequence(core::u32 index, core::u32 base) noexcept;

[[nodiscard]] SubpixelJitter compute_taa_jitter(core::u64 frame_index,
                                                core::u32 sample_count) noexcept;

[[nodiscard]] core::Status register_taa_pass(render_graph::RenderGraph& graph,
                                             render_graph::ResourceHandle scene_color,
                                             render_graph::ResourceHandle history_color,
                                             render_graph::ResourceHandle resolved_color,
                                             render_graph::PassHandle& out_pass) noexcept;

} // namespace gameengine::renderer::advanced
