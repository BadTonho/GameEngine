#pragma once

#include <string_view>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"

namespace gameengine::rhi {

enum class BackendKind : core::u8 {
    auto_select = 0,
    vulkan,
    d3d12,
    metal,
};

enum class ShaderTarget : core::u8 {
    spirv = 0,
    dxil,
    metal,
};

struct BackendCapabilities final {
    BackendKind backend = BackendKind::auto_select;
    ShaderTarget shader_target = ShaderTarget::spirv;
    std::string_view backend_name = "none";
    std::string_view device_name = "unavailable";
    bool compiled = false;
    bool compute_supported = false;
    bool indirect_draw_supported = false;
    bool multi_draw_indirect_supported = false;
    bool gpu_timestamps_supported = false;
    bool shadow_map_supported = false;
    bool environment_map_supported = false;
    bool ray_tracing_supported = false;
    bool mesh_shaders_supported = false;
    bool validation_supported = false;
    bool validation_enabled = false;
    core::u32 max_frames_in_flight = 2U;
    core::u64 max_vram_budget_bytes = 0U;
};

[[nodiscard]] constexpr std::string_view backend_name(BackendKind kind) noexcept
{
    switch (kind) {
    case BackendKind::auto_select:
        return "auto";
    case BackendKind::vulkan:
        return "vulkan";
    case BackendKind::d3d12:
        return "d3d12";
    case BackendKind::metal:
        return "metal";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view shader_target_name(ShaderTarget target) noexcept
{
    switch (target) {
    case ShaderTarget::spirv:
        return "spirv";
    case ShaderTarget::dxil:
        return "dxil";
    case ShaderTarget::metal:
        return "metal";
    }
    return "unknown";
}

[[nodiscard]] core::Status parse_backend_kind(std::string_view value,
                                              BackendKind& out_kind) noexcept;

[[nodiscard]] BackendKind resolve_default_backend() noexcept;

[[nodiscard]] bool is_backend_compiled(BackendKind kind) noexcept;

[[nodiscard]] BackendCapabilities query_static_backend_capabilities(BackendKind kind) noexcept;

} // namespace gameengine::rhi
