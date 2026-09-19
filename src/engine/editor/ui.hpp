#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "engine/core/types.hpp"
#include "engine/editor/asset_catalog.hpp"
#include "engine/editor/console.hpp"
#include "engine/editor/profiler.hpp"
#include "engine/editor/project.hpp"
#include "engine/editor/ui_types.hpp"

namespace gameengine::editor {

struct UiLayout final {
    core::f32 hierarchy_width = 260.0F;
    core::f32 inspector_width = 300.0F;
    core::f32 bottom_panel_height = 190.0F;
    core::f32 row_height = 24.0F;
    core::f32 padding = 12.0F;
};

enum class EditorPanel : core::u8 {
    assets = 0,
    console,
    profiler,
};

class UiState final {
public:
    void build(const ProjectSession& project,
               core::u32 width,
               core::u32 height,
               std::vector<UiVertex>& vertices);
    void build(const ProjectSession& project,
               const AssetCatalog& catalog,
               const ConsoleBuffer& console,
               const EditorProfiler& profiler,
               core::u32 width,
               core::u32 height,
               std::vector<UiVertex>& vertices);
    [[nodiscard]] core::Status click(ProjectSession& project,
                                      core::f32 x,
                                      core::f32 y,
                                      core::u32 width,
                                      core::u32 height) const noexcept;
    [[nodiscard]] core::Status click(ProjectSession& project,
                                      AssetCatalog& catalog,
                                      core::f32 x,
                                      core::f32 y,
                                      core::u32 width,
                                      core::u32 height) noexcept;

    [[nodiscard]] EditorPanel active_panel() const noexcept { return active_panel_; }
    [[nodiscard]] bool consume_refresh_request() noexcept
    {
        const bool requested = refresh_requested_;
        refresh_requested_ = false;
        return requested;
    }

    [[nodiscard]] const UiLayout& layout() const noexcept { return layout_; }
    [[nodiscard]] std::span<const scene::Entity> entity_order() const noexcept
    {
        return entity_order_;
    }

private:
    void build_internal(const ProjectSession& project,
                        const AssetCatalog* catalog,
                        const ConsoleBuffer* console,
                        const EditorProfiler* profiler,
                        core::u32 width,
                        core::u32 height,
                        std::vector<UiVertex>& vertices);
    [[nodiscard]] core::Status click_internal(ProjectSession& project,
                                               AssetCatalog* catalog,
                                               core::f32 x,
                                               core::f32 y,
                                               core::u32 width,
                                               core::u32 height) noexcept;
    void add_rect(std::vector<UiVertex>& vertices,
                  core::f32 left,
                  core::f32 top,
                  core::f32 right,
                  core::f32 bottom,
                  core::u32 width,
                  core::u32 height,
                  const std::array<core::f32, 4>& color) const;
    void add_text(std::vector<UiVertex>& vertices,
                  std::string_view text,
                  core::f32 x,
                  core::f32 y,
                  core::f32 scale,
                  core::u32 width,
                  core::u32 height,
                  const std::array<core::f32, 4>& color) const;

    UiLayout layout_{};
    std::vector<scene::Entity> entity_order_{};
    EditorPanel active_panel_ = EditorPanel::assets;
    bool refresh_requested_ = false;
};

} // namespace gameengine::editor
