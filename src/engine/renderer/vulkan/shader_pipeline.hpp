#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "engine/core/types.hpp"

namespace gameengine::renderer::vulkan {

enum class ShaderStage : core::u8 {
    vertex = 0,
    fragment,
    compute,
};

enum ShaderCapability : core::u32 {
    shader_capability_none = 0,
    shader_capability_vulkan_1_0 = 1U << 0U,
    shader_capability_vulkan_1_1 = 1U << 1U,
};

struct ShaderArtifact final {
    std::string_view id{};
    ShaderStage stage = ShaderStage::vertex;
    std::string_view entry_point{};
    core::u32 required_capabilities = shader_capability_none;
    core::u32 quality = 0;
    const std::uint32_t* spirv = nullptr;
    core::usize spirv_word_count = 0;
};

struct ShaderDeviceCapabilities final {
    core::u32 supported_capabilities = shader_capability_none;
};

[[nodiscard]] bool shader_capabilities_supported(
    const ShaderArtifact& artifact,
    ShaderDeviceCapabilities capabilities) noexcept;

[[nodiscard]] const ShaderArtifact* select_shader_variant(
    std::span<const ShaderArtifact> variants,
    ShaderDeviceCapabilities capabilities) noexcept;

} // namespace gameengine::renderer::vulkan
