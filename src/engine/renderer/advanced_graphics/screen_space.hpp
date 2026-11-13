#pragma once

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/render_graph/render_graph.hpp"

namespace gameengine::renderer::advanced {

[[nodiscard]] core::Status register_ssao_pass(render_graph::RenderGraph& graph,
                                              render_graph::ResourceHandle depth_buffer,
                                              render_graph::ResourceHandle normal_buffer,
                                              render_graph::ResourceHandle occlusion_out,
                                              render_graph::PassHandle& out_pass) noexcept;

[[nodiscard]] core::Status register_ssr_pass(render_graph::RenderGraph& graph,
                                             render_graph::ResourceHandle scene_color,
                                             render_graph::ResourceHandle depth_buffer,
                                             render_graph::ResourceHandle normal_buffer,
                                             render_graph::ResourceHandle reflection_out,
                                             render_graph::PassHandle& out_pass) noexcept;

[[nodiscard]] core::Status register_contact_shadows_pass(
    render_graph::RenderGraph& graph,
    render_graph::ResourceHandle depth_buffer,
    render_graph::ResourceHandle contact_shadow_out,
    render_graph::PassHandle& out_pass) noexcept;

} // namespace gameengine::renderer::advanced
