#pragma once

#include <span>

#include "engine/core/status.hpp"
#include "engine/rhi/rhi.hpp"
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/scene_bridge.hpp"
#include "engine/scene/scene.hpp"
#include "engine/editor/ui_types.hpp"

namespace gameengine::editor::renderer_bridge {

[[nodiscard]] core::Status set_prepared_assets(
    const rhi::Renderer& renderer,
    const gameengine::renderer::scene_bridge::PreparedRenderAssets& assets) noexcept;
[[nodiscard]] core::Status clear_prepared_assets(const rhi::Renderer& renderer) noexcept;
[[nodiscard]] core::Status attach_scene(const rhi::Renderer& renderer,
                                         scene::Scene& scene) noexcept;
[[nodiscard]] core::Status detach_scene(const rhi::Renderer& renderer) noexcept;
[[nodiscard]] core::Status set_ui_vertices(const rhi::Renderer& renderer,
                                            std::span<const UiVertex> vertices) noexcept;
[[nodiscard]] core::Status read_metrics(
    const rhi::Renderer& renderer,
    gameengine::renderer::metrics::FrameTimingReport& report) noexcept;

} // namespace gameengine::editor::renderer_bridge
