#pragma once

#include <array>
#include <string_view>

#include "engine/core/types.hpp"
#include "engine/renderer/renderer_quality.hpp"
#include "engine/rhi/rhi_capabilities.hpp"

#ifndef GAMEENGINE_HAS_TAA
#define GAMEENGINE_HAS_TAA 1
#endif
#ifndef GAMEENGINE_HAS_UPSCALING
#define GAMEENGINE_HAS_UPSCALING 1
#endif
#ifndef GAMEENGINE_HAS_SSAO
#define GAMEENGINE_HAS_SSAO 1
#endif
#ifndef GAMEENGINE_HAS_SSR
#define GAMEENGINE_HAS_SSR 1
#endif
#ifndef GAMEENGINE_HAS_CONTACT_SHADOWS
#define GAMEENGINE_HAS_CONTACT_SHADOWS 1
#endif
#ifndef GAMEENGINE_HAS_VOLUMETRIC_FOG
#define GAMEENGINE_HAS_VOLUMETRIC_FOG 1
#endif
#ifndef GAMEENGINE_HAS_GPU_PARTICLES
#define GAMEENGINE_HAS_GPU_PARTICLES 1
#endif
#ifndef GAMEENGINE_HAS_REALTIME_GI
#define GAMEENGINE_HAS_REALTIME_GI 1
#endif
#ifndef GAMEENGINE_HAS_RAY_TRACING
#define GAMEENGINE_HAS_RAY_TRACING 1
#endif
#ifndef GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY
#define GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY 1
#endif

namespace gameengine::renderer::advanced {

enum class AdvancedFeatureId : core::u8 {
    taa = 0,
    upscaling,
    ssao,
    ssr,
    contact_shadows,
    volumetric_fog,
    gpu_particles,
    realtime_gi,
    ray_tracing,
    virtualized_geometry,
};

inline constexpr core::usize advanced_feature_count = 10U;

enum class FallbackReason : core::u8 {
    none = 0,
    disabled_by_config,
    not_compiled,
    quality_tier,
    missing_compute,
    missing_ray_tracing,
    missing_mesh_shaders,
    vram_budget,
};

struct AdvancedFeatureConfig final {
    bool enable_taa = true;
    bool enable_upscaling = true;
    bool enable_ssao = true;
    bool enable_ssr = true;
    bool enable_contact_shadows = true;
    bool enable_volumetric_fog = true;
    bool enable_gpu_particles = true;
    bool enable_realtime_gi = false;
    bool enable_ray_tracing = false;
    bool enable_virtualized_geometry = false;

    float render_scale = 0.75F;
    core::u32 taa_jitter_samples = 8U;
    core::u32 ssao_samples = 16U;
    core::u32 ssr_max_steps = 24U;
    core::u32 contact_shadow_steps = 12U;
    core::u32 volumetric_froxel_depth = 64U;
    core::u32 gpu_particle_capacity = 65536U;
    core::u32 gi_probe_grid_dim = 16U;
    core::u32 virtual_cluster_capacity = 16384U;

    [[nodiscard]] static constexpr AdvancedFeatureConfig preset_for_quality(
        quality::RendererQuality quality) noexcept
    {
        AdvancedFeatureConfig cfg{};
        switch (quality) {
        case quality::RendererQuality::low:
            cfg.enable_taa = false;
            cfg.enable_upscaling = false;
            cfg.enable_ssao = false;
            cfg.enable_ssr = false;
            cfg.enable_contact_shadows = false;
            cfg.enable_volumetric_fog = false;
            cfg.enable_gpu_particles = false;
            cfg.enable_realtime_gi = false;
            cfg.enable_ray_tracing = false;
            cfg.enable_virtualized_geometry = false;
            cfg.render_scale = 1.0F;
            break;
        case quality::RendererQuality::medium:
            cfg.enable_taa = true;
            cfg.enable_upscaling = true;
            cfg.enable_ssao = true;
            cfg.enable_ssr = false;
            cfg.enable_contact_shadows = true;
            cfg.enable_volumetric_fog = false;
            cfg.enable_gpu_particles = true;
            cfg.enable_realtime_gi = false;
            cfg.enable_ray_tracing = false;
            cfg.enable_virtualized_geometry = false;
            cfg.render_scale = 0.75F;
            break;
        case quality::RendererQuality::high:
            cfg.enable_taa = true;
            cfg.enable_upscaling = true;
            cfg.enable_ssao = true;
            cfg.enable_ssr = true;
            cfg.enable_contact_shadows = true;
            cfg.enable_volumetric_fog = true;
            cfg.enable_gpu_particles = true;
            cfg.enable_realtime_gi = true;
            cfg.enable_ray_tracing = true;
            cfg.enable_virtualized_geometry = true;
            cfg.render_scale = 0.85F;
            break;
        }
        return cfg;
    }
};

struct AdvancedFeatureCost final {
    core::u64 estimated_vram_bytes = 0;
    core::u64 estimated_ram_bytes = 0;
    core::u64 estimated_cpu_ns = 0;
    core::u64 estimated_gpu_ns = 0;
};

struct AdvancedFeatureStatus final {
    AdvancedFeatureId id = AdvancedFeatureId::taa;
    bool compiled = false;
    bool requested = false;
    bool active = false;
    FallbackReason fallback = FallbackReason::none;
    AdvancedFeatureCost cost{};
};

struct AdvancedGraphicsResolution final {
    std::array<AdvancedFeatureStatus, advanced_feature_count> features{};
    core::u32 active_count = 0;
    core::u32 fallback_count = 0;
    core::u64 total_vram_bytes = 0;
    core::u64 total_ram_bytes = 0;
    core::u64 total_estimated_cpu_ns = 0;
    core::u64 total_estimated_gpu_ns = 0;

    [[nodiscard]] const AdvancedFeatureStatus& status(AdvancedFeatureId id) const noexcept
    {
        return features[static_cast<core::usize>(id)];
    }

    [[nodiscard]] bool is_active(AdvancedFeatureId id) const noexcept
    {
        return status(id).active;
    }
};

[[nodiscard]] constexpr std::string_view feature_name(AdvancedFeatureId id) noexcept
{
    switch (id) {
    case AdvancedFeatureId::taa:
        return "taa";
    case AdvancedFeatureId::upscaling:
        return "upscaling";
    case AdvancedFeatureId::ssao:
        return "ssao";
    case AdvancedFeatureId::ssr:
        return "ssr";
    case AdvancedFeatureId::contact_shadows:
        return "contact_shadows";
    case AdvancedFeatureId::volumetric_fog:
        return "volumetric_fog";
    case AdvancedFeatureId::gpu_particles:
        return "gpu_particles";
    case AdvancedFeatureId::realtime_gi:
        return "realtime_gi";
    case AdvancedFeatureId::ray_tracing:
        return "ray_tracing";
    case AdvancedFeatureId::virtualized_geometry:
        return "virtualized_geometry";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view fallback_reason_name(FallbackReason reason) noexcept
{
    switch (reason) {
    case FallbackReason::none:
        return "none";
    case FallbackReason::disabled_by_config:
        return "disabled_by_config";
    case FallbackReason::not_compiled:
        return "not_compiled";
    case FallbackReason::quality_tier:
        return "quality_tier";
    case FallbackReason::missing_compute:
        return "missing_compute";
    case FallbackReason::missing_ray_tracing:
        return "missing_ray_tracing";
    case FallbackReason::missing_mesh_shaders:
        return "missing_mesh_shaders";
    case FallbackReason::vram_budget:
        return "vram_budget";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view fallback_target_name(AdvancedFeatureId id) noexcept
{
    switch (id) {
    case AdvancedFeatureId::taa:
        return "no_temporal_aa";
    case AdvancedFeatureId::upscaling:
        return "native_resolution";
    case AdvancedFeatureId::ssao:
        return "ambient_hemisphere_only";
    case AdvancedFeatureId::ssr:
        return "environment_cubemap_only";
    case AdvancedFeatureId::contact_shadows:
        return "standard_shadow_map";
    case AdvancedFeatureId::volumetric_fog:
        return "analytic_distance_fog";
    case AdvancedFeatureId::gpu_particles:
        return "cpu_or_disabled";
    case AdvancedFeatureId::realtime_gi:
        return "baked_ambient_probes";
    case AdvancedFeatureId::ray_tracing:
        return "contact_shadows_and_ssr";
    case AdvancedFeatureId::virtualized_geometry:
        return "forward_plus_instanced_lods";
    }
    return "disabled";
}

[[nodiscard]] constexpr bool is_feature_compiled(AdvancedFeatureId id) noexcept
{
    switch (id) {
    case AdvancedFeatureId::taa:
        return GAMEENGINE_HAS_TAA != 0;
    case AdvancedFeatureId::upscaling:
        return GAMEENGINE_HAS_UPSCALING != 0;
    case AdvancedFeatureId::ssao:
        return GAMEENGINE_HAS_SSAO != 0;
    case AdvancedFeatureId::ssr:
        return GAMEENGINE_HAS_SSR != 0;
    case AdvancedFeatureId::contact_shadows:
        return GAMEENGINE_HAS_CONTACT_SHADOWS != 0;
    case AdvancedFeatureId::volumetric_fog:
        return GAMEENGINE_HAS_VOLUMETRIC_FOG != 0;
    case AdvancedFeatureId::gpu_particles:
        return GAMEENGINE_HAS_GPU_PARTICLES != 0;
    case AdvancedFeatureId::realtime_gi:
        return GAMEENGINE_HAS_REALTIME_GI != 0;
    case AdvancedFeatureId::ray_tracing:
        return GAMEENGINE_HAS_RAY_TRACING != 0;
    case AdvancedFeatureId::virtualized_geometry:
        return GAMEENGINE_HAS_VIRTUALIZED_GEOMETRY != 0;
    }
    return false;
}

[[nodiscard]] core::u64 estimate_feature_vram_bytes(AdvancedFeatureId id,
                                                    core::u32 width,
                                                    core::u32 height,
                                                    const AdvancedFeatureConfig& config) noexcept;

[[nodiscard]] AdvancedFeatureCost estimate_feature_cost(AdvancedFeatureId id,
                                                        core::u32 width,
                                                        core::u32 height,
                                                        const AdvancedFeatureConfig& config) noexcept;

[[nodiscard]] AdvancedGraphicsResolution resolve_advanced_features(
    const AdvancedFeatureConfig& config,
    quality::RendererQuality quality,
    const rhi::BackendCapabilities& capabilities,
    core::u32 width,
    core::u32 height,
    core::u64 available_vram_budget_bytes) noexcept;

} // namespace gameengine::renderer::advanced
