#include "engine/renderer/vulkan/shader_pipeline.hpp"

namespace gameengine::renderer::vulkan {

bool shader_capabilities_supported(const ShaderArtifact& artifact,
                                   ShaderDeviceCapabilities capabilities) noexcept
{
    return (artifact.required_capabilities & ~capabilities.supported_capabilities) == 0;
}

const ShaderArtifact* select_shader_variant(std::span<const ShaderArtifact> variants,
                                            ShaderDeviceCapabilities capabilities) noexcept
{
    const ShaderArtifact* selected = nullptr;
    for (const ShaderArtifact& candidate : variants) {
        if (!shader_capabilities_supported(candidate, capabilities)) {
            continue;
        }
        if (selected == nullptr || candidate.quality > selected->quality ||
            (candidate.quality == selected->quality && candidate.id < selected->id)) {
            selected = &candidate;
        }
    }
    return selected;
}

} // namespace gameengine::renderer::vulkan
