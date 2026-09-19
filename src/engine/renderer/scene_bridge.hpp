#pragma once

#include "engine/core/status.hpp"
#include "engine/rhi/rhi.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::renderer::scene_bridge {

[[nodiscard]] core::Status attach_scene(const rhi::Renderer& renderer,
                                         scene::Scene& scene) noexcept;
[[nodiscard]] core::Status detach_scene(const rhi::Renderer& renderer) noexcept;

} // namespace gameengine::renderer::scene_bridge
