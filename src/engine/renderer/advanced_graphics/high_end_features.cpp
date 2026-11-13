#include "engine/renderer/advanced_graphics/high_end_features.hpp"

#include <algorithm>
#include <array>

#include "engine/renderer/advanced_graphics/advanced_features.hpp"

namespace gameengine::renderer::advanced {

core::u32 compute_gi_probe_count(core::u32 grid_dim) noexcept
{
    const core::u32 d = std::clamp(grid_dim, 4U, 64U);
    return d * d * d;
}

core::u32 compute_cluster_dispatch_groups(core::u32 cluster_count) noexcept
{
    return (cluster_count + 63U) / 64U;
}

core::Status register_realtime_gi_pass(render_graph::RenderGraph& graph,
                                       render_graph::ResourceHandle scene_color,
                                       render_graph::ResourceHandle gi_probes_out,
                                       render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_REALTIME_GI
    const std::array<render_graph::ResourceHandle, 1> reads = {scene_color};
    const std::array<render_graph::ResourceHandle, 1> writes = {gi_probes_out};
    render_graph::PassDescription desc{};
    desc.name = "realtime_gi";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)scene_color;
    (void)gi_probes_out;
    out_pass = {};
    return core::Status::ok();
#endif
}

core::Status register_ray_tracing_pass(render_graph::RenderGraph& graph,
                                       render_graph::ResourceHandle depth_buffer,
                                       render_graph::ResourceHandle rt_out,
                                       render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_RAY_TRACING
    const std::array<render_graph::ResourceHandle, 1> reads = {depth_buffer};
    const std::array<render_graph::ResourceHandle, 1> writes = {rt_out};
    render_graph::PassDescription desc{};
    desc.name = "ray_tracing";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)depth_buffer;
    (void)rt_out;
    out_pass = {};
    return core::Status::ok();
#endif
}

core::Status register_virtualized_geometry_pass(render_graph::RenderGraph& graph,
                                                render_graph::ResourceHandle cluster_buffer,
                                                render_graph::ResourceHandle visible_clusters_out,
                                                render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY
    const std::array<render_graph::ResourceHandle, 1> reads = {cluster_buffer};
    const std::array<render_graph::ResourceHandle, 1> writes = {visible_clusters_out};
    render_graph::PassDescription desc{};
    desc.name = "virtualized_geometry";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)cluster_buffer;
    (void)visible_clusters_out;
    out_pass = {};
    return core::Status::ok();
#endif
}

} // namespace gameengine::renderer::advanced
