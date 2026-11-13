#pragma once

#include <span>

#include "engine/assets/prepared_assets.hpp"
#include "engine/core/status.hpp"
#include "engine/rhi/rhi.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::renderer::scene_bridge {

struct PreparedRenderAssets final {
    std::span<const assets::PreparedMesh> meshes{};
    std::span<const assets::PreparedTexture> textures{};
    std::span<const assets::PreparedMaterial> materials{};
};

[[nodiscard]] inline PreparedRenderAssets make_prepared_render_assets(
    const assets::PreparedAssetBundle& bundle) noexcept
{
    return PreparedRenderAssets{
        .meshes = bundle.meshes(),
        .textures = bundle.textures(),
        .materials = bundle.materials(),
    };
}

[[nodiscard]] core::Status set_prepared_assets(const rhi::Renderer& renderer,
                                                const PreparedRenderAssets& assets) noexcept;
[[nodiscard]] core::Status clear_prepared_assets(const rhi::Renderer& renderer) noexcept;
[[nodiscard]] core::Status attach_scene(const rhi::Renderer& renderer,
                                         scene::Scene& scene) noexcept;
[[nodiscard]] core::Status detach_scene(const rhi::Renderer& renderer) noexcept;

} // namespace gameengine::renderer::scene_bridge
