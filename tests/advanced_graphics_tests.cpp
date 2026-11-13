#include <cmath>
#include <cstdio>

#include "engine/renderer/advanced_graphics/advanced_features.hpp"
#include "engine/renderer/advanced_graphics/advanced_pipeline.hpp"
#include "engine/renderer/advanced_graphics/high_end_features.hpp"
#include "engine/renderer/advanced_graphics/temporal_aa.hpp"
#include "engine/renderer/advanced_graphics/upscaling.hpp"
#include "engine/renderer/advanced_graphics/volumetrics_and_particles.hpp"

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "advanced_graphics_tests failure: %s\n", message);
    return 1;
}

gameengine::rhi::BackendCapabilities make_caps(bool compute,
                                               bool ray_tracing,
                                               bool mesh_shaders) noexcept
{
    gameengine::rhi::BackendCapabilities caps{};
    caps.compiled = true;
    caps.compute_supported = compute;
    caps.ray_tracing_supported = ray_tracing;
    caps.mesh_shaders_supported = mesh_shaders;
    caps.max_vram_budget_bytes = 512ULL * 1024ULL * 1024ULL;
    return caps;
}

} // namespace

int main()
{
    using namespace gameengine::renderer::advanced;
    using gameengine::renderer::quality::RendererQuality;

    // 1. Feature names & fallback targets cover all 10 features in roadmap order.
    for (gameengine::core::usize i = 0; i < advanced_feature_count; ++i) {
        const auto id = static_cast<AdvancedFeatureId>(i);
        if (feature_name(id) == "unknown" || fallback_target_name(id).empty()) {
            return fail("missing feature name or fallback target");
        }
        const auto words = feature_spirv_words(id);
        if (is_feature_compiled(id)) {
            if (words.empty() || words[0] != 0x07230203U) {
                return fail("compiled advanced feature missing valid SPIR-V artifact");
            }
        } else {
            if (!words.empty()) {
                return fail("disabled advanced feature should have zero SPIR-V words");
            }
        }
    }

    // 2. Low quality disables all advanced features cleanly.
    {
        const auto caps = make_caps(true, true, true);
        const auto cfg = AdvancedFeatureConfig::preset_for_quality(RendererQuality::high);
        const auto res = resolve_advanced_features(
            cfg, RendererQuality::low, caps, 1920U, 1080U, caps.max_vram_budget_bytes);
        if (res.active_count != 0U || res.total_vram_bytes != 0ULL) {
            return fail("low quality must disable all advanced features");
        }
        const auto expected_low_fallback =
            is_feature_compiled(AdvancedFeatureId::taa) ? FallbackReason::quality_tier
                                                        : FallbackReason::not_compiled;
        if (res.status(AdvancedFeatureId::taa).fallback != expected_low_fallback) {
            return fail("low quality must report quality_tier (or not_compiled) fallback");
        }
    }

    // 3. Missing compute falls back cleanly.
    {
        const auto caps = make_caps(false, false, false);
        const auto cfg = AdvancedFeatureConfig::preset_for_quality(RendererQuality::medium);
        const auto res = resolve_advanced_features(
            cfg, RendererQuality::medium, caps, 1920U, 1080U, caps.max_vram_budget_bytes);
        if (res.active_count != 0U) {
            return fail("missing compute must disable compute-based advanced features");
        }
        if (is_feature_compiled(AdvancedFeatureId::taa) &&
            res.status(AdvancedFeatureId::taa).fallback != FallbackReason::missing_compute) {
            return fail("taa must report missing_compute when compute is unavailable");
        }
    }

    // 4. High quality without hardware RT/mesh shaders falls back for RT & virtualized geometry.
    {
        const auto caps = make_caps(true, false, false);
        const auto cfg = AdvancedFeatureConfig::preset_for_quality(RendererQuality::high);
        const auto res = resolve_advanced_features(
            cfg, RendererQuality::high, caps, 1920U, 1080U, caps.max_vram_budget_bytes);
        if (is_feature_compiled(AdvancedFeatureId::ray_tracing) &&
            res.status(AdvancedFeatureId::ray_tracing).fallback !=
                FallbackReason::missing_ray_tracing) {
            return fail("ray tracing must fall back when hardware RT is unavailable");
        }
        if (is_feature_compiled(AdvancedFeatureId::virtualized_geometry) &&
            res.status(AdvancedFeatureId::virtualized_geometry).fallback !=
                FallbackReason::missing_mesh_shaders) {
            return fail("virtualized geometry must fall back when mesh shaders are unavailable");
        }

        gameengine::renderer::render_graph::RenderGraph graph;
        if (!build_advanced_render_graph(res, graph) || !graph.compiled()) {
            return fail("render graph failed to compile for high quality without RT");
        }
    }

    // 5. High quality with full hardware capabilities activates all compiled features.
    {
        const auto caps = make_caps(true, true, true);
        const auto cfg = AdvancedFeatureConfig::preset_for_quality(RendererQuality::high);
        const auto res = resolve_advanced_features(
            cfg, RendererQuality::high, caps, 1920U, 1080U, caps.max_vram_budget_bytes);

        gameengine::core::u32 expected_compiled = 0U;
        for (gameengine::core::usize i = 0; i < advanced_feature_count; ++i) {
            if (is_feature_compiled(static_cast<AdvancedFeatureId>(i))) {
                ++expected_compiled;
            }
        }
        if (res.active_count != expected_compiled) {
            return fail("expected all compiled features active on full-capability hardware");
        }

        gameengine::renderer::render_graph::RenderGraph graph;
        if (!build_advanced_render_graph(res, graph) || !graph.compiled()) {
            return fail("render graph failed to compile with all features active");
        }
        if (graph.execution_order().size() != expected_compiled) {
            return fail("render graph pass count mismatch");
        }
    }

    // 6. Tight VRAM budget triggers vram_budget fallback.
    {
        const auto caps = make_caps(true, true, true);
        const auto cfg = AdvancedFeatureConfig::preset_for_quality(RendererQuality::high);
        const auto res = resolve_advanced_features(
            cfg, RendererQuality::high, caps, 1920U, 1080U, 1024ULL);
        if (res.active_count != 0U) {
            return fail("tight 1KB VRAM budget must trigger vram_budget fallback");
        }
    }

    // 7. Subsystem math helpers (compiled only when the corresponding feature is enabled).
    {
#if GAMEENGINE_HAS_TAA
        const auto j0 = compute_taa_jitter(0ULL, 8U);
        const auto j1 = compute_taa_jitter(1ULL, 8U);
        const auto j8 = compute_taa_jitter(8ULL, 8U);
        if (std::fabs(j0.x - j8.x) > 1e-6F || std::fabs(j0.y - j8.y) > 1e-6F ||
            (std::fabs(j0.x - j1.x) < 1e-4F && std::fabs(j0.y - j1.y) < 1e-4F)) {
            return fail("Halton TAA jitter sequence invalid");
        }
#endif

#if GAMEENGINE_HAS_UPSCALING
        const auto ext = compute_upscale_input_extent(1920U, 1080U, 0.5F);
        if (ext.width != 960U || ext.height != 540U) {
            return fail("upscale extent calculation mismatch");
        }
#endif

#if GAMEENGINE_HAS_VOLUMETRIC_FOG
        const auto froxels = compute_froxel_grid(1280U, 720U, 64U);
        if (froxels.width != 160U || froxels.height != 90U || froxels.depth != 64U) {
            return fail("froxel grid dimensions mismatch");
        }
#endif

#if GAMEENGINE_HAS_GPU_PARTICLES
        if (compute_particle_dispatch_groups(65U) != 2U) {
            return fail("particle dispatch helper mismatch");
        }
#endif

#if GAMEENGINE_HAS_REALTIME_GI
        if (compute_gi_probe_count(8U) != 512U) {
            return fail("GI probe helper mismatch");
        }
#endif

#if GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY
        if (compute_cluster_dispatch_groups(129U) != 3U) {
            return fail("cluster dispatch helper mismatch");
        }
#endif
    }

    return 0;
}
