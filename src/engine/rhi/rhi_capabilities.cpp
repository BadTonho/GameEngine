#include "engine/rhi/rhi_capabilities.hpp"

namespace gameengine::rhi {

core::Status parse_backend_kind(std::string_view value, BackendKind& out_kind) noexcept
{
    if (value == "auto") {
        out_kind = BackendKind::auto_select;
        return core::Status{};
    }
    if (value == "vulkan") {
        out_kind = BackendKind::vulkan;
        return core::Status{};
    }
    if (value == "d3d12" || value == "dx12") {
        out_kind = BackendKind::d3d12;
        return core::Status{};
    }
    if (value == "metal") {
        out_kind = BackendKind::metal;
        return core::Status{};
    }
    return core::Status{core::ErrorCode::invalid_argument};
}

BackendKind resolve_default_backend() noexcept
{
#if defined(__APPLE__) && defined(GAMEENGINE_RENDERER_HAS_METAL) && GAMEENGINE_RENDERER_HAS_METAL
    return BackendKind::metal;
#elif defined(GAMEENGINE_RENDERER_HAS_VULKAN) && GAMEENGINE_RENDERER_HAS_VULKAN
    return BackendKind::vulkan;
#elif defined(_WIN32) && defined(GAMEENGINE_RENDERER_HAS_D3D12) && GAMEENGINE_RENDERER_HAS_D3D12
    return BackendKind::d3d12;
#elif defined(GAMEENGINE_RENDERER_HAS_METAL) && GAMEENGINE_RENDERER_HAS_METAL
    return BackendKind::metal;
#else
    return BackendKind::vulkan;
#endif
}

bool is_backend_compiled(BackendKind kind) noexcept
{
    const BackendKind resolved = kind == BackendKind::auto_select ? resolve_default_backend() : kind;
    switch (resolved) {
    case BackendKind::vulkan:
#if defined(GAMEENGINE_RENDERER_HAS_VULKAN) && GAMEENGINE_RENDERER_HAS_VULKAN
        return true;
#else
        return false;
#endif
    case BackendKind::d3d12:
#if defined(GAMEENGINE_RENDERER_HAS_D3D12) && GAMEENGINE_RENDERER_HAS_D3D12
        return true;
#else
        return false;
#endif
    case BackendKind::metal:
#if defined(GAMEENGINE_RENDERER_HAS_METAL) && GAMEENGINE_RENDERER_HAS_METAL
        return true;
#else
        return false;
#endif
    case BackendKind::auto_select:
        return false;
    }
    return false;
}

BackendCapabilities query_static_backend_capabilities(BackendKind kind) noexcept
{
    const BackendKind resolved = kind == BackendKind::auto_select ? resolve_default_backend() : kind;
    switch (resolved) {
    case BackendKind::vulkan:
        return BackendCapabilities{
            .backend = BackendKind::vulkan,
            .shader_target = ShaderTarget::spirv,
            .backend_name = "vulkan",
            .device_name = "vulkan-1.0-baseline",
            .compiled = is_backend_compiled(BackendKind::vulkan),
            .compute_supported = true,
            .indirect_draw_supported = true,
            .multi_draw_indirect_supported = true,
            .gpu_timestamps_supported = true,
            .shadow_map_supported = true,
            .environment_map_supported = true,
            .validation_supported = true,
            .validation_enabled = false,
            .max_frames_in_flight = 2U,
            .max_vram_budget_bytes = 512ULL * 1024ULL * 1024ULL,
        };
    case BackendKind::d3d12:
        return BackendCapabilities{
            .backend = BackendKind::d3d12,
            .shader_target = ShaderTarget::dxil,
            .backend_name = "d3d12",
            .device_name = "d3d12-sm6.0-baseline",
            .compiled = is_backend_compiled(BackendKind::d3d12),
            .compute_supported = true,
            .indirect_draw_supported = true,
            .multi_draw_indirect_supported = true,
            .gpu_timestamps_supported = true,
            .shadow_map_supported = true,
            .environment_map_supported = true,
            .validation_supported = true,
            .validation_enabled = false,
            .max_frames_in_flight = 2U,
            .max_vram_budget_bytes = 512ULL * 1024ULL * 1024ULL,
        };
    case BackendKind::metal:
        return BackendCapabilities{
            .backend = BackendKind::metal,
            .shader_target = ShaderTarget::metal,
            .backend_name = "metal",
            .device_name = "metal-apple-baseline",
            .compiled = is_backend_compiled(BackendKind::metal),
            .compute_supported = true,
            .indirect_draw_supported = true,
            .multi_draw_indirect_supported = false,
            .gpu_timestamps_supported = true,
            .shadow_map_supported = true,
            .environment_map_supported = true,
            .validation_supported = true,
            .validation_enabled = false,
            .max_frames_in_flight = 2U,
            .max_vram_budget_bytes = 512ULL * 1024ULL * 1024ULL,
        };
    case BackendKind::auto_select:
        return BackendCapabilities{};
    }
    return BackendCapabilities{};
}

} // namespace gameengine::rhi
