#pragma once

#include <span>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/advanced_graphics/advanced_features.hpp"
#include "engine/renderer/render_graph/render_graph.hpp"

namespace gameengine::renderer::advanced {

[[nodiscard]] std::span<const core::u32> feature_spirv_words(AdvancedFeatureId id) noexcept;

[[nodiscard]] core::Status build_advanced_render_graph(
    const AdvancedGraphicsResolution& resolution,
    render_graph::RenderGraph& graph) noexcept;

} // namespace gameengine::renderer::advanced
