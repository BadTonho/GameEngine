#include "engine/editor/project.hpp"
#include "engine/editor/ui.hpp"

#include <cstring>
#include <filesystem>
#include <vector>

namespace {

bool equal_vertices(const std::vector<gameengine::editor::UiVertex>& left,
                    const std::vector<gameengine::editor::UiVertex>& right) noexcept
{
    return left.size() == right.size() &&
           std::memcmp(left.data(), right.data(), left.size() * sizeof(left.front())) == 0;
}

} // namespace

int main()
{
    using namespace gameengine::editor;
    std::error_code error;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path(error) / "gameengine_editor_ui_test_project";
    if (error) {
        return 1;
    }
    std::filesystem::remove_all(directory, error);
    ProjectSession project;
    if (!project.create_new(directory)) {
        return 2;
    }

    UiState ui;
    std::vector<UiVertex> first;
    std::vector<UiVertex> second;
    ui.build(project, 1280U, 720U, first);
    ui.build(project, 1280U, 720U, second);
    if (sizeof(UiVertex) != 32U || first.empty() || first.size() % 6U != 0U ||
        !equal_vertices(first, second) || ui.entity_order().empty()) {
        std::filesystem::remove_all(directory, error);
        return 3;
    }
    const AnimationUiState animation{
        true, true, "procedural_cube", 0.5F, 2.0F};
    ui.build(project, AssetCatalog{}, ConsoleBuffer{}, EditorProfiler{}, animation, 1280U, 720U,
             first);
    ui.build(project, AssetCatalog{}, ConsoleBuffer{}, EditorProfiler{}, animation, 1280U, 720U,
             second);
    if (first.empty() || !equal_vertices(first, second)) {
        std::filesystem::remove_all(directory, error);
        return 4;
    }
    const auto selected_before = project.selected_entity();
    if (!ui.click(project, 8.0F, 52.0F, 1280U, 720U).ok() ||
        project.selected_entity() != ui.entity_order().front() ||
        ui.click(project, -1.0F, 52.0F, 1280U, 720U).code !=
            gameengine::core::ErrorCode::invalid_argument) {
        std::filesystem::remove_all(directory, error);
        return 5;
    }
    if (!project.select(selected_before)) {
        std::filesystem::remove_all(directory, error);
        return 6;
    }
    std::filesystem::remove_all(directory, error);
    return 0;
}
