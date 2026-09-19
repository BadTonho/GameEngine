#include "engine/editor/project.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool expect(bool value) noexcept
{
    return value;
}

} // namespace

int main()
{
    using gameengine::editor::ProjectManifest;
    using gameengine::editor::ProjectSession;

    ProjectManifest manifest;
    if (!expect(gameengine::editor::parse_project_manifest(
                    "{\"format\":1,\"name\":\"Demo\",\"scene\":\"scenes/main.gescene\"}",
                    manifest).ok()) ||
        !expect(manifest.name == "Demo") ||
        !expect(manifest.renderer_quality ==
                gameengine::renderer::quality::RendererQuality::medium) ||
        !expect(manifest.window_width == 1280U && manifest.window_height == 720U)) {
        return 1;
    }
    if (!expect(!gameengine::editor::parse_project_manifest(
                    "{\"format\":2,\"name\":\"Demo\",\"scene\":\"main.gescene\"}",
                    manifest).ok()) ||
        !expect(!gameengine::editor::parse_project_manifest(
                    "{\"format\":1,\"name\":\"Demo\",\"scene\":\"../main.gescene\"}",
                    manifest).ok()) ||
        !expect(!gameengine::editor::parse_project_manifest(
                    "{\"format\":1,\"name\":\"Demo\",\"scene\":\"C:/main.gescene\"}",
                    manifest).ok()) ||
        !expect(!gameengine::editor::parse_project_manifest(
                    "{\"format\":1,\"name\":\"Demo\",\"scene\":\"main.gescene\","
                    "\"renderer_quality\":\"low\",\"renderer_quality\":\"high\"}",
                    manifest).ok()) ||
        !expect(!gameengine::editor::parse_project_manifest(
                    "{\"format\":1,\"name\":\"Demo\",\"scene\":\"main.gescene\","
                    "\"renderer_quality\":\"ultra\"}",
                    manifest)) ||
        !expect(!gameengine::editor::parse_project_manifest(
                    "{\"format\":1,\"name\":\"Demo\",\"scene\":12}", manifest).ok())) {
        return 2;
    }
    if (!expect(gameengine::editor::parse_project_manifest(
                    "{\n  \"format\": 1,\n  \"name\": \"Bootstrap\",\n"
                    "  \"scene\": \"scenes/main.gescene\",\n"
                    "  \"renderer_quality\": \"medium\",\n"
                    "  \"window\": {\n    \"width\": 1280,\n    \"height\": 720\n  }\n}\n",
                    manifest)
                    .ok())) {
        return 8;
    }
    const std::string first = gameengine::editor::serialize_project_manifest(manifest);
    const std::string second = gameengine::editor::serialize_project_manifest(manifest);
    if (!expect(first == second) || !expect(first.find("\"format\": 1") != std::string::npos)) {
        return 3;
    }

    std::error_code error;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path(error) / "gameengine_editor_test_project";
    if (error) {
        return 4;
    }
    std::filesystem::remove_all(directory, error);
    ProjectSession created;
    if (!expect(created.create_new(directory).ok()) ||
        !expect(std::filesystem::exists(created.project_path())) ||
        !expect(std::filesystem::exists(created.scene_path())) ||
        !expect(created.selected_entity().valid())) {
        std::filesystem::remove_all(directory, error);
        return 5;
    }
    const auto selected = created.selected_entity();
    if (!expect(created.move_selected(1.0F, 0.0F, 0.0F).ok()) ||
        !expect(created.dirty()) || !expect(created.reset_selected().ok()) ||
        !expect(created.save().ok())) {
        std::filesystem::remove_all(directory, error);
        return 6;
    }
    ProjectSession loaded;
    if (!expect(loaded.load(created.project_path()).ok()) ||
        !expect(loaded.selected_entity() == selected) || !expect(!loaded.dirty())) {
        std::filesystem::remove_all(directory, error);
        return 7;
    }
    const auto previous_selected = loaded.selected_entity();
    const std::filesystem::path invalid_project = directory / "invalid.geproject";
    std::ofstream invalid_file(invalid_project, std::ios::binary | std::ios::trunc);
    invalid_file << "{\"format\":1,\"name\":\"Broken\",\"scene\":\"missing.gescene\"}";
    invalid_file.close();
    if (!expect(!loaded.load(invalid_project).ok()) ||
        !expect(loaded.selected_entity() == previous_selected)) {
        std::filesystem::remove_all(directory, error);
        return 9;
    }
    std::filesystem::remove_all(directory, error);
    return 0;
}
