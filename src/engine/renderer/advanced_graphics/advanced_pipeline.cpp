#include "engine/renderer/advanced_graphics/advanced_pipeline.hpp"

#include "engine/renderer/advanced_graphics/advanced_shaders.hpp"
#if GAMEENGINE_HAS_REALTIME_GI || GAMEENGINE_HAS_RAY_TRACING || GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY
#include "engine/renderer/advanced_graphics/high_end_features.hpp"
#endif
#if GAMEENGINE_HAS_SSAO || GAMEENGINE_HAS_SSR || GAMEENGINE_HAS_CONTACT_SHADOWS
#include "engine/renderer/advanced_graphics/screen_space.hpp"
#endif
#if GAMEENGINE_HAS_TAA
#include "engine/renderer/advanced_graphics/temporal_aa.hpp"
#endif
#if GAMEENGINE_HAS_UPSCALING
#include "engine/renderer/advanced_graphics/upscaling.hpp"
#endif
#if GAMEENGINE_HAS_VOLUMETRIC_FOG || GAMEENGINE_HAS_GPU_PARTICLES
#include "engine/renderer/advanced_graphics/volumetrics_and_particles.hpp"
#endif

namespace gameengine::renderer::advanced {

std::span<const core::u32> feature_spirv_words(AdvancedFeatureId id) noexcept
{
    switch (id) {
    case AdvancedFeatureId::taa:
#if GAMEENGINE_HAS_TAA
        return {shaders::taa_spirv, shaders::taa_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::upscaling:
#if GAMEENGINE_HAS_UPSCALING
        return {shaders::upscaling_spirv, shaders::upscaling_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::ssao:
#if GAMEENGINE_HAS_SSAO
        return {shaders::ssao_spirv, shaders::ssao_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::ssr:
#if GAMEENGINE_HAS_SSR
        return {shaders::ssr_spirv, shaders::ssr_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::contact_shadows:
#if GAMEENGINE_HAS_CONTACT_SHADOWS
        return {shaders::contact_shadows_spirv, shaders::contact_shadows_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::volumetric_fog:
#if GAMEENGINE_HAS_VOLUMETRIC_FOG
        return {shaders::volumetric_fog_spirv, shaders::volumetric_fog_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::gpu_particles:
#if GAMEENGINE_HAS_GPU_PARTICLES
        return {shaders::gpu_particles_spirv, shaders::gpu_particles_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::realtime_gi:
#if GAMEENGINE_HAS_REALTIME_GI
        return {shaders::realtime_gi_spirv, shaders::realtime_gi_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::ray_tracing:
#if GAMEENGINE_HAS_RAY_TRACING
        return {shaders::ray_tracing_spirv, shaders::ray_tracing_spirv_word_count};
#else
        return {};
#endif
    case AdvancedFeatureId::virtualized_geometry:
#if GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY
        return {shaders::virtualized_geometry_spirv,
                shaders::virtualized_geometry_spirv_word_count};
#else
        return {};
#endif
    }
    return {};
}

core::Status build_advanced_render_graph(const AdvancedGraphicsResolution& resolution,
                                         render_graph::RenderGraph& graph) noexcept
{
    graph.reset();

    render_graph::ResourceHandle scene_color{};
    render_graph::ResourceHandle depth_buffer{};
    render_graph::ResourceHandle normal_buffer{};
    render_graph::ResourceHandle history_color{};
    render_graph::ResourceHandle taa_resolved{};
    render_graph::ResourceHandle upscaled_color{};
    render_graph::ResourceHandle ssao_out{};
    render_graph::ResourceHandle ssr_out{};
    render_graph::ResourceHandle contact_shadow_out{};
    render_graph::ResourceHandle froxel_out{};
    render_graph::ResourceHandle particle_buffer{};
    render_graph::ResourceHandle gi_probes_out{};
    render_graph::ResourceHandle rt_out{};
    render_graph::ResourceHandle cluster_buffer{};
    render_graph::ResourceHandle visible_clusters_out{};

    if (const auto s = graph.add_resource(
            {"scene_color", render_graph::ResourceKind::color_attachment, true}, scene_color);
        !s.ok()) {
        return s;
    }
    if (const auto s = graph.add_resource(
            {"depth_buffer", render_graph::ResourceKind::depth_attachment, true}, depth_buffer);
        !s.ok()) {
        return s;
    }
    if (const auto s = graph.add_resource(
            {"normal_buffer", render_graph::ResourceKind::color_attachment, true}, normal_buffer);
        !s.ok()) {
        return s;
    }

    render_graph::PassHandle pass{};
#if GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY
    if (resolution.is_active(AdvancedFeatureId::virtualized_geometry)) {
        if (const auto s = graph.add_resource(
                {"virtual_clusters", render_graph::ResourceKind::storage_buffer, true},
                cluster_buffer);
            !s.ok()) {
            return s;
        }
        if (const auto s = graph.add_resource(
                {"visible_clusters", render_graph::ResourceKind::storage_buffer, false},
                visible_clusters_out);
            !s.ok()) {
            return s;
        }
        if (const auto s = register_virtualized_geometry_pass(
                graph, cluster_buffer, visible_clusters_out, pass);
            !s.ok()) {
            return s;
        }
    }
#else
    (void)cluster_buffer;
    (void)visible_clusters_out;
#endif

#if GAMEENGINE_HAS_GPU_PARTICLES
    if (resolution.is_active(AdvancedFeatureId::gpu_particles)) {
        if (const auto s = graph.add_resource(
                {"gpu_particles_buf", render_graph::ResourceKind::storage_buffer, false},
                particle_buffer);
            !s.ok()) {
            return s;
        }
        if (const auto s = register_gpu_particles_pass(graph, particle_buffer, pass); !s.ok()) {
            return s;
        }
    }
#else
    (void)particle_buffer;
#endif

#if GAMEENGINE_HAS_SSAO
    if (resolution.is_active(AdvancedFeatureId::ssao)) {
        if (const auto s = graph.add_resource(
                {"ssao_occlusion", render_graph::ResourceKind::storage_buffer, false}, ssao_out);
            !s.ok()) {
            return s;
        }
        if (const auto s = register_ssao_pass(graph, depth_buffer, normal_buffer, ssao_out, pass);
            !s.ok()) {
            return s;
        }
    }
#else
    (void)ssao_out;
#endif

#if GAMEENGINE_HAS_CONTACT_SHADOWS
    if (resolution.is_active(AdvancedFeatureId::contact_shadows)) {
        if (const auto s = graph.add_resource(
                {"contact_shadow_mask", render_graph::ResourceKind::storage_buffer, false},
                contact_shadow_out);
            !s.ok()) {
            return s;
        }
        if (const auto s =
                register_contact_shadows_pass(graph, depth_buffer, contact_shadow_out, pass);
            !s.ok()) {
            return s;
        }
    }
#else
    (void)contact_shadow_out;
#endif

#if GAMEENGINE_HAS_SSR
    if (resolution.is_active(AdvancedFeatureId::ssr)) {
        if (const auto s = graph.add_resource(
                {"ssr_reflection", render_graph::ResourceKind::storage_buffer, false}, ssr_out);
            !s.ok()) {
            return s;
        }
        if (const auto s = register_ssr_pass(
                graph, scene_color, depth_buffer, normal_buffer, ssr_out, pass);
            !s.ok()) {
            return s;
        }
    }
#else
    (void)ssr_out;
#endif

#if GAMEENGINE_HAS_VOLUMETRIC_FOG
    if (resolution.is_active(AdvancedFeatureId::volumetric_fog)) {
        if (const auto s = graph.add_resource(
                {"volumetric_froxels", render_graph::ResourceKind::storage_buffer, false},
                froxel_out);
            !s.ok()) {
            return s;
        }
        if (const auto s =
                register_volumetric_fog_pass(graph, depth_buffer, froxel_out, pass);
            !s.ok()) {
            return s;
        }
    }
#else
    (void)froxel_out;
#endif

#if GAMEENGINE_HAS_REALTIME_GI
    if (resolution.is_active(AdvancedFeatureId::realtime_gi)) {
        if (const auto s = graph.add_resource(
                {"gi_probes", render_graph::ResourceKind::storage_buffer, false}, gi_probes_out);
            !s.ok()) {
            return s;
        }
        if (const auto s = register_realtime_gi_pass(graph, scene_color, gi_probes_out, pass);
            !s.ok()) {
            return s;
        }
    }
#else
    (void)gi_probes_out;
#endif

#if GAMEENGINE_HAS_RAY_TRACING
    if (resolution.is_active(AdvancedFeatureId::ray_tracing)) {
        if (const auto s = graph.add_resource(
                {"rt_shadow_reflection", render_graph::ResourceKind::storage_buffer, false},
                rt_out);
            !s.ok()) {
            return s;
        }
        if (const auto s = register_ray_tracing_pass(graph, depth_buffer, rt_out, pass); !s.ok()) {
            return s;
        }
    }
#else
    (void)rt_out;
#endif
    (void)pass;

    render_graph::ResourceHandle current_color = scene_color;
    if (resolution.is_active(AdvancedFeatureId::taa)) {
        if (const auto s = graph.add_resource(
                {"taa_history", render_graph::ResourceKind::color_attachment, true},
                history_color);
            !s.ok()) {
            return s;
        }
        if (const auto s = graph.add_resource(
                {"taa_resolved", render_graph::ResourceKind::color_attachment, false},
                taa_resolved);
            !s.ok()) {
            return s;
        }
#if GAMEENGINE_HAS_TAA
        if (const auto s = register_taa_pass(
                graph, current_color, history_color, taa_resolved, pass);
            !s.ok()) {
            return s;
        }
        current_color = taa_resolved;
#endif
    }

    if (resolution.is_active(AdvancedFeatureId::upscaling)) {
        if (const auto s = graph.add_resource(
                {"upscaled_color", render_graph::ResourceKind::color_attachment, false},
                upscaled_color);
            !s.ok()) {
            return s;
        }
#if GAMEENGINE_HAS_UPSCALING
        if (const auto s = register_upscale_pass(graph, current_color, upscaled_color, pass);
            !s.ok()) {
            return s;
        }
#endif
    }

    return graph.compile();
}

} // namespace gameengine::renderer::advanced
