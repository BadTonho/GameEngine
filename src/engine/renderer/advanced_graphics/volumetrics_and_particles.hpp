#pragma once

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/render_graph/render_graph.hpp"

namespace gameengine::renderer::advanced {

struct FroxelGridDimensions final {
    core::u32 width = 160U;
    core::u32 height = 90U;
    core::u32 depth = 64U;

    [[nodiscard]] constexpr core::u64 voxel_count() const noexcept
    {
        return static_cast<core::u64>(width) * static_cast<core::u64>(height) *
               static_cast<core::u64>(depth);
    }
};

[[nodiscard]] FroxelGridDimensions compute_froxel_grid(core::u32 viewport_width,
                                                       core::u32 viewport_height,
                                                       core::u32 depth_slices) noexcept;

[[nodiscard]] core::u32 compute_particle_dispatch_groups(core::u32 max_particles) noexcept;

[[nodiscard]] core::Status register_volumetric_fog_pass(render_graph::RenderGraph& graph,
                                                        render_graph::ResourceHandle depth_buffer,
                                                        render_graph::ResourceHandle froxel_out,
                                                        render_graph::PassHandle& out_pass) noexcept;

[[nodiscard]] core::Status register_gpu_particles_pass(render_graph::RenderGraph& graph,
                                                       render_graph::ResourceHandle particle_buffer,
                                                       render_graph::PassHandle& out_pass) noexcept;

} // namespace gameengine::renderer::advanced
