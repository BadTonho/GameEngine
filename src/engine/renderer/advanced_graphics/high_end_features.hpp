#pragma once

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/render_graph/render_graph.hpp"

namespace gameengine::renderer::advanced {

[[nodiscard]] core::u32 compute_gi_probe_count(core::u32 grid_dim) noexcept;

[[nodiscard]] core::u32 compute_cluster_dispatch_groups(core::u32 cluster_count) noexcept;

[[nodiscard]] core::Status register_realtime_gi_pass(render_graph::RenderGraph& graph,
                                                     render_graph::ResourceHandle scene_color,
                                                     render_graph::ResourceHandle gi_probes_out,
                                                     render_graph::PassHandle& out_pass) noexcept;

[[nodiscard]] core::Status register_ray_tracing_pass(render_graph::RenderGraph& graph,
                                                     render_graph::ResourceHandle depth_buffer,
                                                     render_graph::ResourceHandle rt_out,
                                                     render_graph::PassHandle& out_pass) noexcept;

[[nodiscard]] core::Status register_virtualized_geometry_pass(
    render_graph::RenderGraph& graph,
    render_graph::ResourceHandle cluster_buffer,
    render_graph::ResourceHandle visible_clusters_out,
    render_graph::PassHandle& out_pass) noexcept;

} // namespace gameengine::renderer::advanced
