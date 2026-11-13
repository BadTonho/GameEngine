#include "engine/renderer/advanced_graphics/screen_space.hpp"

#include <array>

#include "engine/renderer/advanced_graphics/advanced_features.hpp"

namespace gameengine::renderer::advanced {

core::Status register_ssao_pass(render_graph::RenderGraph& graph,
                                render_graph::ResourceHandle depth_buffer,
                                render_graph::ResourceHandle normal_buffer,
                                render_graph::ResourceHandle occlusion_out,
                                render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_SSAO
    const std::array<render_graph::ResourceHandle, 2> reads = {depth_buffer, normal_buffer};
    const std::array<render_graph::ResourceHandle, 1> writes = {occlusion_out};
    render_graph::PassDescription desc{};
    desc.name = "ssao";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)depth_buffer;
    (void)normal_buffer;
    (void)occlusion_out;
    out_pass = {};
    return core::Status::ok();
#endif
}

core::Status register_ssr_pass(render_graph::RenderGraph& graph,
                               render_graph::ResourceHandle scene_color,
                               render_graph::ResourceHandle depth_buffer,
                               render_graph::ResourceHandle normal_buffer,
                               render_graph::ResourceHandle reflection_out,
                               render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_SSR
    const std::array<render_graph::ResourceHandle, 3> reads = {
        scene_color, depth_buffer, normal_buffer};
    const std::array<render_graph::ResourceHandle, 1> writes = {reflection_out};
    render_graph::PassDescription desc{};
    desc.name = "ssr";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)scene_color;
    (void)depth_buffer;
    (void)normal_buffer;
    (void)reflection_out;
    out_pass = {};
    return core::Status::ok();
#endif
}

core::Status register_contact_shadows_pass(render_graph::RenderGraph& graph,
                                           render_graph::ResourceHandle depth_buffer,
                                           render_graph::ResourceHandle contact_shadow_out,
                                           render_graph::PassHandle& out_pass) noexcept
{
#if GAMEENGINE_HAS_CONTACT_SHADOWS
    const std::array<render_graph::ResourceHandle, 1> reads = {depth_buffer};
    const std::array<render_graph::ResourceHandle, 1> writes = {contact_shadow_out};
    render_graph::PassDescription desc{};
    desc.name = "contact_shadows";
    desc.reads = reads;
    desc.writes = writes;
    desc.dispatch_calls = 1U;
    return graph.add_pass(desc, out_pass);
#else
    (void)graph;
    (void)depth_buffer;
    (void)contact_shadow_out;
    out_pass = {};
    return core::Status::ok();
#endif
}

} // namespace gameengine::renderer::advanced
