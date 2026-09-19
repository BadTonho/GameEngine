#include "engine/editor/ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace gameengine::editor {

namespace {

[[nodiscard]] std::array<core::u8, 7> glyph(char character) noexcept
{
    switch (character) {
    case 'A': return {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    case 'C': return {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e};
    case 'D': return {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e};
    case 'E': return {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f};
    case 'G': return {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f};
    case 'I': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    case 'N': return {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11};
    case 'O': return {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    case 'P': return {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
    case 'R': return {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    case 'S': return {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    case 'T': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04};
    case 'X': return {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04};
    case '0': return {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e};
    case '1': return {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e};
    case '2': return {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f};
    case '3': return {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e};
    case '4': return {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02};
    case '5': return {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e};
    case '6': return {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e};
    case '7': return {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e};
    case '9': return {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e};
    case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c};
    case '-': return {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    default: return {0, 0, 0, 0, 0, 0, 0};
    }
}

[[nodiscard]] core::f32 to_ndc_x(core::f32 value, core::u32 width) noexcept
{
    return value / static_cast<core::f32>(width) * 2.0F - 1.0F;
}

[[nodiscard]] core::f32 to_ndc_y(core::f32 value, core::u32 height) noexcept
{
    return 1.0F - value / static_cast<core::f32>(height) * 2.0F;
}

} // namespace

void UiState::build(const ProjectSession& project,
                    core::u32 width,
                    core::u32 height,
                    std::vector<UiVertex>& vertices)
{
    vertices.clear();
    entity_order_.assign(project.scene().entities().begin(), project.scene().entities().end());
    std::sort(entity_order_.begin(), entity_order_.end(), [](scene::Entity left, scene::Entity right) {
        return left.index() < right.index();
    });
    if (width == 0U || height == 0U) {
        return;
    }
    const core::f32 right_panel = static_cast<core::f32>(width) - layout_.inspector_width;
    add_rect(vertices,
             0.0F,
             0.0F,
             layout_.hierarchy_width,
             static_cast<core::f32>(height),
             width,
             height,
             {0.035F, 0.045F, 0.075F, 0.94F});
    add_rect(vertices,
             right_panel,
             0.0F,
             static_cast<core::f32>(width),
             static_cast<core::f32>(height),
             width,
             height,
             {0.035F, 0.045F, 0.075F, 0.94F});
    add_text(vertices,
             "SCENE",
             layout_.padding,
             layout_.padding,
             2.0F,
             width,
             height,
             {0.75F, 0.86F, 1.0F, 1.0F});
    add_text(vertices,
             "INSPECTOR",
             right_panel + layout_.padding,
             layout_.padding,
             2.0F,
             width,
             height,
             {0.75F, 0.86F, 1.0F, 1.0F});
    for (std::size_t index = 0; index < entity_order_.size(); ++index) {
        const core::f32 top = 42.0F + static_cast<core::f32>(index) * layout_.row_height;
        const bool selected = entity_order_[index] == project.selected_entity();
        add_rect(vertices,
                 4.0F,
                 top,
                 layout_.hierarchy_width - 4.0F,
                 top + layout_.row_height - 2.0F,
                 width,
                 height,
                 selected ? std::array<core::f32, 4>{0.15F, 0.35F, 0.65F, 0.95F}
                          : std::array<core::f32, 4>{0.07F, 0.09F, 0.14F, 0.95F});
        add_text(vertices,
                 "ENTITY",
                 layout_.padding,
                 top + 5.0F,
                 1.25F,
                 width,
                 height,
                 {0.88F, 0.92F, 1.0F, 1.0F});
        add_text(vertices,
                 std::to_string(entity_order_[index].index()),
                 126.0F,
                 top + 5.0F,
                 1.25F,
                 width,
                 height,
                 {0.65F, 0.75F, 0.9F, 1.0F});
    }
    const scene::TransformComponent* transform = project.scene().transform(project.selected_entity());
    if (transform != nullptr) {
        const auto line = [this, &vertices, width, height, right_panel](std::string_view label,
                                                                           core::f32 value,
                                                                           core::f32 row) {
            add_text(vertices,
                     label,
                     right_panel + layout_.padding,
                     row,
                     1.3F,
                     width,
                     height,
                     {0.76F, 0.82F, 0.92F, 1.0F});
            add_text(vertices,
                     std::to_string(static_cast<int>(std::round(value * 100.0F))),
                     right_panel + 112.0F,
                     row,
                     1.3F,
                     width,
                     height,
                     {0.95F, 0.76F, 0.4F, 1.0F});
        };
        line("X", transform->local_position.x, 64.0F);
        line("Y", transform->local_position.y, 86.0F);
        line("Z", transform->local_position.z, 108.0F);
    }
    add_text(vertices,
             project.dirty() ? "SAVE" : "READY",
             right_panel + layout_.padding,
             static_cast<core::f32>(height) - 32.0F,
             1.3F,
             width,
             height,
             project.dirty() ? std::array<core::f32, 4>{1.0F, 0.65F, 0.25F, 1.0F}
                             : std::array<core::f32, 4>{0.35F, 0.9F, 0.55F, 1.0F});
}

core::Status UiState::click(ProjectSession& project,
                            core::f32 x,
                            core::f32 y,
                            core::u32 width,
                            core::u32 height) const noexcept
{
    if (width == 0U || height == 0U || x < 0.0F || x >= layout_.hierarchy_width || y < 42.0F) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const core::usize row = static_cast<core::usize>((y - 42.0F) / layout_.row_height);
    if (row >= entity_order_.size()) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    return project.select(entity_order_[row]);
}

void UiState::add_rect(std::vector<UiVertex>& vertices,
                       core::f32 left,
                       core::f32 top,
                       core::f32 right,
                       core::f32 bottom,
                       core::u32 width,
                       core::u32 height,
                       const std::array<core::f32, 4>& color) const
{
    const UiVertex a{{to_ndc_x(left, width), to_ndc_y(top, height)}, {0.0F, 0.0F}, color};
    const UiVertex b{{to_ndc_x(right, width), to_ndc_y(top, height)}, {1.0F, 0.0F}, color};
    const UiVertex c{{to_ndc_x(right, width), to_ndc_y(bottom, height)}, {1.0F, 1.0F}, color};
    const UiVertex d{{to_ndc_x(left, width), to_ndc_y(bottom, height)}, {0.0F, 1.0F}, color};
    vertices.insert(vertices.end(), {a, b, c, a, c, d});
}

void UiState::add_text(std::vector<UiVertex>& vertices,
                       std::string_view text,
                       core::f32 x,
                       core::f32 y,
                       core::f32 scale,
                       core::u32 width,
                       core::u32 height,
                       const std::array<core::f32, 4>& color) const
{
    core::f32 cursor = x;
    for (const char character : text) {
        const auto rows = glyph(character);
        for (core::u32 row = 0; row < rows.size(); ++row) {
            for (core::u32 column = 0; column < 5U; ++column) {
                if ((rows[row] & (1U << (4U - column))) != 0U) {
                    add_rect(vertices,
                             cursor + static_cast<core::f32>(column) * scale,
                             y + static_cast<core::f32>(row) * scale,
                             cursor + static_cast<core::f32>(column + 1U) * scale,
                             y + static_cast<core::f32>(row + 1U) * scale,
                             width,
                             height,
                             color);
                }
            }
        }
        cursor += 6.0F * scale;
    }
}

} // namespace gameengine::editor
