#include "engine/renderer/advanced_graphics/advanced_features.hpp"

#include <algorithm>

namespace gameengine::renderer::advanced {
namespace {

[[nodiscard]] bool is_requested(const AdvancedFeatureConfig& config,
                                AdvancedFeatureId id) noexcept
{
    switch (id) {
    case AdvancedFeatureId::taa:
        return config.enable_taa;
    case AdvancedFeatureId::upscaling:
        return config.enable_upscaling;
    case AdvancedFeatureId::ssao:
        return config.enable_ssao;
    case AdvancedFeatureId::ssr:
        return config.enable_ssr;
    case AdvancedFeatureId::contact_shadows:
        return config.enable_contact_shadows;
    case AdvancedFeatureId::volumetric_fog:
        return config.enable_volumetric_fog;
    case AdvancedFeatureId::gpu_particles:
        return config.enable_gpu_particles;
    case AdvancedFeatureId::realtime_gi:
        return config.enable_realtime_gi;
    case AdvancedFeatureId::ray_tracing:
        return config.enable_ray_tracing;
    case AdvancedFeatureId::virtualized_geometry:
        return config.enable_virtualized_geometry;
    }
    return false;
}

[[nodiscard]] bool allowed_by_quality(AdvancedFeatureId id,
                                      quality::RendererQuality quality) noexcept
{
    if (quality == quality::RendererQuality::low) {
        return false;
    }
    if (quality == quality::RendererQuality::medium) {
        switch (id) {
        case AdvancedFeatureId::taa:
        case AdvancedFeatureId::upscaling:
        case AdvancedFeatureId::ssao:
        case AdvancedFeatureId::contact_shadows:
        case AdvancedFeatureId::gpu_particles:
            return true;
        case AdvancedFeatureId::ssr:
        case AdvancedFeatureId::volumetric_fog:
        case AdvancedFeatureId::realtime_gi:
        case AdvancedFeatureId::ray_tracing:
        case AdvancedFeatureId::virtualized_geometry:
            return false;
        }
    }
    return true;
}

} // namespace

core::u64 estimate_feature_vram_bytes(AdvancedFeatureId id,
                                      core::u32 width,
                                      core::u32 height,
                                      const AdvancedFeatureConfig& config) noexcept
{
    const core::u64 pixels = static_cast<core::u64>(std::max(width, 1U)) *
                             static_cast<core::u64>(std::max(height, 1U));
    switch (id) {
    case AdvancedFeatureId::taa:
        // Two RGBA16F history ping-pong targets (8 bytes per pixel each).
        return pixels * 16ULL;
    case AdvancedFeatureId::upscaling:
        // Output RGBA16F resolve buffer.
        return pixels * 8ULL;
    case AdvancedFeatureId::ssao:
        // R8 occlusion buffer + blurred ping-pong.
        return pixels * 2ULL;
    case AdvancedFeatureId::ssr:
        // Half-res RGBA16F reflection target.
        return (pixels / 4ULL) * 8ULL + 4096ULL;
    case AdvancedFeatureId::contact_shadows:
        // R8 contact shadow mask.
        return pixels;
    case AdvancedFeatureId::volumetric_fog: {
        const core::u64 froxels = 160ULL * 90ULL *
                                  static_cast<core::u64>(std::max(config.volumetric_froxel_depth, 1U));
        return froxels * 16ULL;
    }
    case AdvancedFeatureId::gpu_particles:
        return static_cast<core::u64>(config.gpu_particle_capacity) * 32ULL + 64ULL;
    case AdvancedFeatureId::realtime_gi: {
        const core::u64 dim = static_cast<core::u64>(std::max(config.gi_probe_grid_dim, 1U));
        return dim * dim * dim * 32ULL;
    }
    case AdvancedFeatureId::ray_tracing:
        // RT shadow/reflection accumulation target + scratch buffer.
        return pixels * 8ULL + (4ULL * 1024ULL * 1024ULL);
    case AdvancedFeatureId::virtualized_geometry:
        // Cluster metadata + visible cluster index buffer.
        return static_cast<core::u64>(config.virtual_cluster_capacity) * 36ULL;
    }
    return 0ULL;
}

AdvancedFeatureCost estimate_feature_cost(AdvancedFeatureId id,
                                          core::u32 width,
                                          core::u32 height,
                                          const AdvancedFeatureConfig& config) noexcept
{
    AdvancedFeatureCost cost{};
    cost.estimated_vram_bytes = estimate_feature_vram_bytes(id, width, height, config);
    switch (id) {
    case AdvancedFeatureId::taa:
        cost.estimated_ram_bytes = 256U;
        cost.estimated_cpu_ns = 12000ULL;
        cost.estimated_gpu_ns = 350000ULL;
        break;
    case AdvancedFeatureId::upscaling:
        cost.estimated_ram_bytes = 128U;
        cost.estimated_cpu_ns = 8000ULL;
        cost.estimated_gpu_ns = 250000ULL;
        break;
    case AdvancedFeatureId::ssao:
        cost.estimated_ram_bytes = 512U;
        cost.estimated_cpu_ns = 15000ULL;
        cost.estimated_gpu_ns = 650000ULL;
        break;
    case AdvancedFeatureId::ssr:
        cost.estimated_ram_bytes = 256U;
        cost.estimated_cpu_ns = 14000ULL;
        cost.estimated_gpu_ns = 900000ULL;
        break;
    case AdvancedFeatureId::contact_shadows:
        cost.estimated_ram_bytes = 128U;
        cost.estimated_cpu_ns = 10000ULL;
        cost.estimated_gpu_ns = 300000ULL;
        break;
    case AdvancedFeatureId::volumetric_fog:
        cost.estimated_ram_bytes = 512U;
        cost.estimated_cpu_ns = 18000ULL;
        cost.estimated_gpu_ns = 850000ULL;
        break;
    case AdvancedFeatureId::gpu_particles:
        cost.estimated_ram_bytes = 1024U;
        cost.estimated_cpu_ns = 12000ULL;
        cost.estimated_gpu_ns = 220000ULL;
        break;
    case AdvancedFeatureId::realtime_gi:
        cost.estimated_ram_bytes = 2048U;
        cost.estimated_cpu_ns = 25000ULL;
        cost.estimated_gpu_ns = 1400000ULL;
        break;
    case AdvancedFeatureId::ray_tracing:
        cost.estimated_ram_bytes = 2048U;
        cost.estimated_cpu_ns = 30000ULL;
        cost.estimated_gpu_ns = 2200000ULL;
        break;
    case AdvancedFeatureId::virtualized_geometry:
        cost.estimated_ram_bytes = 1024U;
        cost.estimated_cpu_ns = 20000ULL;
        cost.estimated_gpu_ns = 750000ULL;
        break;
    }
    return cost;
}

AdvancedGraphicsResolution resolve_advanced_features(
    const AdvancedFeatureConfig& config,
    quality::RendererQuality quality,
    const rhi::BackendCapabilities& capabilities,
    core::u32 width,
    core::u32 height,
    core::u64 available_vram_budget_bytes) noexcept
{
    AdvancedGraphicsResolution result{};
    core::u64 remaining_vram = available_vram_budget_bytes;

    for (core::usize i = 0; i < advanced_feature_count; ++i) {
        const auto id = static_cast<AdvancedFeatureId>(i);
        auto& status = result.features[i];
        status.id = id;
        status.compiled = is_feature_compiled(id);
        status.requested = is_requested(config, id);

        if (!status.requested) {
            status.fallback = FallbackReason::disabled_by_config;
            continue;
        }
        if (!status.compiled) {
            status.fallback = FallbackReason::not_compiled;
            ++result.fallback_count;
            continue;
        }
        if (!allowed_by_quality(id, quality)) {
            status.fallback = FallbackReason::quality_tier;
            ++result.fallback_count;
            continue;
        }
        if (!capabilities.compute_supported) {
            status.fallback = FallbackReason::missing_compute;
            ++result.fallback_count;
            continue;
        }
        if (id == AdvancedFeatureId::ray_tracing && !capabilities.ray_tracing_supported) {
            status.fallback = FallbackReason::missing_ray_tracing;
            ++result.fallback_count;
            continue;
        }
        if (id == AdvancedFeatureId::virtualized_geometry &&
            !capabilities.mesh_shaders_supported) {
            status.fallback = FallbackReason::missing_mesh_shaders;
            ++result.fallback_count;
            continue;
        }

        const AdvancedFeatureCost cost = estimate_feature_cost(id, width, height, config);
        if (available_vram_budget_bytes > 0 && cost.estimated_vram_bytes > remaining_vram) {
            status.fallback = FallbackReason::vram_budget;
            ++result.fallback_count;
            continue;
        }

        if (available_vram_budget_bytes > 0) {
            remaining_vram -= cost.estimated_vram_bytes;
        }
        status.active = true;
        status.fallback = FallbackReason::none;
        status.cost = cost;
        ++result.active_count;
        result.total_vram_bytes += cost.estimated_vram_bytes;
        result.total_ram_bytes += cost.estimated_ram_bytes;
        result.total_estimated_cpu_ns += cost.estimated_cpu_ns;
        result.total_estimated_gpu_ns += cost.estimated_gpu_ns;
    }

    return result;
}

} // namespace gameengine::renderer::advanced
