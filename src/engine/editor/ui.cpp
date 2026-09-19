#include "engine/editor/ui.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

namespace gameengine::editor {

namespace {

[[nodiscard]] std::array<core::u8, 7> glyph(char character) noexcept
{
    switch (character) {
    case 'A': return {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    case 'B': return {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e};
    case 'C': return {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e};
    case 'D': return {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e};
    case 'E': return {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f};
    case 'F': return {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10};
    case 'G': return {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f};
    case 'H': return {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    case 'I': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f};
    case 'J': return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    case 'M': return {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11};
    case 'O': return {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    case 'P': return {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
    case 'Q': return {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d};
    case 'R': return {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    case 'S': return {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    case 'T': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11};
    case 'X': return {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04};
    case 'Z': return {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f};
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
    case '/': return {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
    case '_': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f};
    default: return {0, 0, 0, 0, 0, 0, 0};
    }
}

[[nodiscard]] std::string_view panel_name(EditorPanel panel) noexcept
{
    switch (panel) {
    case EditorPanel::assets:
        return "ASSETS";
    case EditorPanel::console:
        return "CONSOLE";
    case EditorPanel::profiler:
        return "PROFILER";
    }
    return "UNKNOWN";
}

[[nodiscard]] std::string_view console_level_name(core::LogLevel level) noexcept
{
    switch (level) {
    case core::LogLevel::debug:
        return "DEBUG";
    case core::LogLevel::info:
        return "INFO";
    case core::LogLevel::warning:
        return "WARN";
    case core::LogLevel::error:
        return "ERROR";
    }
    return "UNKNOWN";
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
    build_internal(project, nullptr, nullptr, nullptr, width, height, vertices);
}

void UiState::build(const ProjectSession& project,
                    const AssetCatalog& catalog,
                    const ConsoleBuffer& console,
                    const EditorProfiler& profiler,
                    core::u32 width,
                    core::u32 height,
                    std::vector<UiVertex>& vertices)
{
    build_internal(project, &catalog, &console, &profiler, width, height, vertices);
}

void UiState::build_internal(const ProjectSession& project,
                             const AssetCatalog* catalog,
                             const ConsoleBuffer* console,
                             const EditorProfiler* profiler,
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
    const core::f32 right_panel = std::max(
        layout_.hierarchy_width + 1.0F,
        static_cast<core::f32>(width) - layout_.inspector_width);
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
    if (catalog != nullptr && catalog->selected() != nullptr) {
        const AssetRecord& asset = *catalog->selected();
        add_text(vertices,
                 "ASSET INFO",
                 right_panel + layout_.padding,
                 142.0F,
                 1.1F,
                 width,
                 height,
                 {0.75F, 0.86F, 1.0F, 1.0F});
        add_text(vertices,
                 std::string(AssetCatalog::kind_name(asset.kind)) + " " + asset.detail,
                 right_panel + layout_.padding,
                 164.0F,
                 0.78F,
                 width,
                 height,
                 {0.86F, 0.9F, 1.0F, 1.0F});
        add_text(vertices,
                 "ID " + std::to_string(asset.asset_id),
                 right_panel + layout_.padding,
                 182.0F,
                 0.72F,
                 width,
                 height,
                 {0.72F, 0.8F, 0.92F, 1.0F});
        add_text(vertices,
                 "HASH " + std::to_string(asset.content_hash),
                 right_panel + layout_.padding,
                 198.0F,
                 0.62F,
                 width,
                 height,
                 {0.62F, 0.7F, 0.82F, 1.0F});
        add_text(vertices,
                 "BYTES " + std::to_string(asset.byte_size),
                 right_panel + layout_.padding,
                 212.0F,
                 0.62F,
                 width,
                 height,
                 {0.62F, 0.7F, 0.82F, 1.0F});
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

    if (catalog == nullptr || console == nullptr || profiler == nullptr ||
        right_panel <= layout_.hierarchy_width + 24.0F || height < 80U) {
        return;
    }

    const core::f32 panel_left = layout_.hierarchy_width;
    const core::f32 panel_right = right_panel;
    const core::f32 panel_top = std::max(
        0.0F, static_cast<core::f32>(height) - layout_.bottom_panel_height);
    add_rect(vertices,
             panel_left,
             panel_top,
             panel_right,
             static_cast<core::f32>(height),
             width,
             height,
             {0.045F, 0.06F, 0.1F, 0.98F});
    const core::f32 tab_width = (panel_right - panel_left) / 3.0F;
    for (core::u32 index = 0; index < 3U; ++index) {
        const EditorPanel panel = static_cast<EditorPanel>(index);
        const core::f32 left = panel_left + static_cast<core::f32>(index) * tab_width;
        add_rect(vertices,
                 left,
                 panel_top,
                 left + tab_width,
                 panel_top + 26.0F,
                 width,
                 height,
                 panel == active_panel_ ? std::array<core::f32, 4>{0.16F, 0.32F, 0.58F, 1.0F}
                                        : std::array<core::f32, 4>{0.07F, 0.09F, 0.14F, 1.0F});
        add_text(vertices,
                 panel_name(panel),
                 left + 8.0F,
                 panel_top + 7.0F,
                 1.15F,
                 width,
                 height,
                 {0.85F, 0.9F, 1.0F, 1.0F});
    }

    const core::f32 content_top = panel_top + 34.0F;
    const core::u32 max_rows = static_cast<core::u32>(
        std::max(0.0F, (static_cast<core::f32>(height) - content_top - 8.0F) / 22.0F));
    if (active_panel_ == EditorPanel::assets) {
        add_text(vertices,
                 "REFRESH",
                 panel_left + tab_width - 76.0F,
                 panel_top + 7.0F,
                 1.0F,
                 width,
                 height,
                 {1.0F, 0.75F, 0.35F, 1.0F});
        const auto records = catalog->records();
        const core::u32 row_count = std::min(max_rows, static_cast<core::u32>(records.size()));
        for (core::u32 index = 0; index < row_count; ++index) {
            const AssetRecord& record = records[index];
            const core::f32 row_top = content_top + static_cast<core::f32>(index) * 22.0F;
            add_rect(vertices,
                     panel_left + 4.0F,
                     row_top,
                     panel_right - 4.0F,
                     row_top + 20.0F,
                     width,
                     height,
                     index == catalog->selected_index()
                         ? std::array<core::f32, 4>{0.15F, 0.3F, 0.55F, 0.95F}
                         : std::array<core::f32, 4>{0.065F, 0.085F, 0.13F, 0.95F});
            add_text(vertices,
                     record.relative_path,
                     panel_left + 10.0F,
                     row_top + 3.0F,
                     0.85F,
                     width,
                     height,
                     {0.9F, 0.93F, 1.0F, 1.0F});
            add_text(vertices,
                     std::string(AssetCatalog::kind_name(record.kind)) +
                         (record.valid ? " OK" : " INVALID"),
                     panel_left + 10.0F,
                     row_top + 12.0F,
                     0.65F,
                     width,
                     height,
                     record.valid ? std::array<core::f32, 4>{0.35F, 0.9F, 0.55F, 1.0F}
                                  : std::array<core::f32, 4>{1.0F, 0.4F, 0.35F, 1.0F});
        }
        if (records.empty()) {
            add_text(vertices,
                     "NO ASSETS",
                     panel_left + 10.0F,
                     content_top,
                     1.0F,
                     width,
                     height,
                     {0.65F, 0.7F, 0.8F, 1.0F});
        }
    } else if (active_panel_ == EditorPanel::console) {
        const core::u32 count = console->size();
        const core::u32 first = count > max_rows ? count - max_rows : 0U;
        for (core::u32 index = first; index < count; ++index) {
            const ConsoleMessageView message = console->message(index);
            const core::f32 row_top = content_top + static_cast<core::f32>(index - first) * 22.0F;
            add_text(vertices,
                     std::string(console_level_name(message.level)) + ":" +
                         std::string(message.text),
                     panel_left + 10.0F,
                     row_top + 4.0F,
                     0.85F,
                     width,
                     height,
                     message.level == core::LogLevel::error
                         ? std::array<core::f32, 4>{1.0F, 0.42F, 0.35F, 1.0F}
                         : std::array<core::f32, 4>{0.82F, 0.88F, 0.96F, 1.0F});
        }
    } else {
        if (!profiler->has_frame()) {
            add_text(vertices,
                     "PROFILER WAITING",
                     panel_left + 10.0F,
                     content_top,
                     0.95F,
                     width,
                     height,
                     {0.65F, 0.7F, 0.8F, 1.0F});
        } else {
            const auto& report = profiler->frame();
            add_text(vertices,
                     "PASSES",
                     panel_left + 10.0F,
                     content_top,
                     0.9F,
                     width,
                     height,
                     {0.8F, 0.86F, 1.0F, 1.0F});
            const core::u32 pass_rows = std::min(max_rows, report.pass_count);
            for (core::u32 index = 0; index < pass_rows; ++index) {
                const auto& pass = report.passes[index];
                const core::f32 row_top = content_top + 18.0F + static_cast<core::f32>(index) * 22.0F;
                add_text(vertices,
                         std::string(pass.name) + " CPU " +
                             std::to_string(pass.cpu_nanoseconds / 1000U) + "US",
                         panel_left + 10.0F,
                         row_top,
                         0.75F,
                         width,
                         height,
                         {0.86F, 0.9F, 1.0F, 1.0F});
            }
            add_text(vertices,
                     report.gpu_timestamps_available ? "GPU AVAILABLE" : "GPU UNAVAILABLE",
                     panel_left + 10.0F,
                     static_cast<core::f32>(height) - 36.0F,
                     0.8F,
                     width,
                     height,
                     report.gpu_timestamps_available
                         ? std::array<core::f32, 4>{0.35F, 0.9F, 0.55F, 1.0F}
                         : std::array<core::f32, 4>{1.0F, 0.75F, 0.35F, 1.0F});
        }
        const auto memory = profiler->memory();
        add_text(vertices,
                 "STARTUP " + std::to_string(profiler->startup_nanoseconds() / 1'000'000U) +
                     "MS",
                 panel_left + 170.0F,
                 content_top,
                 0.8F,
                 width,
                 height,
                 {0.72F, 0.8F, 0.92F, 1.0F});
        add_text(vertices,
                 memory.current_available ? "RAM " + std::to_string(memory.current_bytes)
                                          : "RAM UNAVAILABLE",
                 panel_left + 170.0F,
                 content_top + 18.0F,
                 0.72F,
                 width,
                 height,
                 {0.72F, 0.8F, 0.92F, 1.0F});
        add_text(vertices,
                 memory.peak_available ? "PEAK " + std::to_string(memory.peak_bytes)
                                       : "PEAK UNAVAILABLE",
                 panel_left + 170.0F,
                 content_top + 34.0F,
                 0.72F,
                 width,
                 height,
                 {0.62F, 0.7F, 0.82F, 1.0F});
    }
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

core::Status UiState::click(ProjectSession& project,
                            AssetCatalog& catalog,
                            core::f32 x,
                            core::f32 y,
                            core::u32 width,
                            core::u32 height) noexcept
{
    return click_internal(project, &catalog, x, y, width, height);
}

core::Status UiState::click_internal(ProjectSession& project,
                                     AssetCatalog* catalog,
                                     core::f32 x,
                                     core::f32 y,
                                     core::u32 width,
                                     core::u32 height) noexcept
{
    if (width == 0U || height == 0U || x < 0.0F || y < 0.0F ||
        x >= static_cast<core::f32>(width) || y >= static_cast<core::f32>(height)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    if (x < layout_.hierarchy_width && y >= 42.0F) {
        const core::usize row = static_cast<core::usize>((y - 42.0F) / layout_.row_height);
        if (row >= entity_order_.size()) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        return project.select(entity_order_[row]);
    }
    const core::f32 right_panel = std::max(
        layout_.hierarchy_width + 1.0F,
        static_cast<core::f32>(width) - layout_.inspector_width);
    if (x < layout_.hierarchy_width || x >= right_panel || height < 80U) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const core::f32 panel_top = std::max(
        0.0F, static_cast<core::f32>(height) - layout_.bottom_panel_height);
    if (y < panel_top || y >= static_cast<core::f32>(height)) {
        return core::Status{core::ErrorCode::invalid_argument};
    }
    const core::f32 tab_width = (right_panel - layout_.hierarchy_width) / 3.0F;
    if (y < panel_top + 30.0F) {
        const core::u32 tab = static_cast<core::u32>(
            (x - layout_.hierarchy_width) / tab_width);
        if (tab >= 3U) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
        active_panel_ = static_cast<EditorPanel>(tab);
        if (active_panel_ == EditorPanel::assets &&
            x >= layout_.hierarchy_width + tab_width - 88.0F) {
            refresh_requested_ = true;
        }
        return {};
    }
    if (active_panel_ == EditorPanel::assets && catalog != nullptr) {
        const core::f32 content_top = panel_top + 34.0F;
        if (y >= content_top) {
            const core::u32 index = static_cast<core::u32>((y - content_top) / 22.0F);
            if (index < catalog->records().size()) {
                return catalog->select(index);
            }
        }
    }
    return core::Status{core::ErrorCode::invalid_argument};
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
        const char normalized = static_cast<char>(
            std::toupper(static_cast<unsigned char>(character)));
        const auto rows = glyph(normalized);
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
