#include "engine/renderer/advanced_graphics/volumetrics_and_particles.hpp"

#include <algorithm>
#include <array>

#include "engine/renderer/advanced_graphics/advanced_features.hpp"

namespace gameengine::renderer::advanced {

FroxelGridDimensions compute_froxel_grid(core::u32 viewport_width,
                                         core::u32 viewport_height,
                                         core::u32 depth_slices) noexcept
{
    return FroxelGridDimensions{
        .width = std::max(viewport_width / 8U, 1U),
        .height = std::max(viewport_height / 8U, 1U),
        .depth = std::clamp(depth_slices, 8U, 128U),
    };
}

core::u32 compute_particle_dispatch_groups(core::u32 max_particles) noexcept
{
    return (max_particles + 63U) / 64U;
}

core::Status register_volumetric_fog_pass(render_graph::RenderGraph& graph,
                                          render_graph::ResourceHandle depth_buffer,
                                          render_graph::ResourceHandle froxel_out,
                                          render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_VOLUMETRIC_FOG
    const std::array<render_graph::ResourceHandle, 1> reads = {depth_buffer};
    const std::array<render_graph::ResourceHandle, 1> writes = {froxel_out};
    render_graph::PassDescription desc{};
    desc.name = "volumetric_fog";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)depth_buffer;
    (void)froxel_out;
    out_pass = {};
    return core::Status::ok();
#endif
}

core::Status register_gpu_particles_pass(render_graph::RenderGraph& graph,
                                         render_graph::ResourceHandle particle_buffer,
                                         render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_GPU_PARTICLES
    const std::array<render_graph::ResourceHandle, 1> writes = {particle_buffer};
    render_graph::PassDescription desc{};
    desc.name = "gpu_particles";
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)particle_buffer;
    out_pass = {};
    return core::Status::ok();
#endif
}

} // namespace gameengine::renderer::advanced
