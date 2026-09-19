#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/renderer_quality.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::editor {

struct ProjectManifest final {
    static constexpr core::u32 format_version = 1U;

    std::string name = "Bootstrap";
    std::string scene = "scenes/main.gescene";
    renderer::quality::RendererQuality renderer_quality =
        renderer::quality::RendererQuality::medium;
    core::u32 window_width = 1280U;
    core::u32 window_height = 720U;
};

class ProjectSession final {
public:
    ProjectSession() noexcept = default;
    ~ProjectSession() noexcept = default;

    ProjectSession(const ProjectSession&) = delete;
    ProjectSession& operator=(const ProjectSession&) = delete;

    [[nodiscard]] core::Status create_new(const std::filesystem::path& directory) noexcept;
    [[nodiscard]] core::Status load(const std::filesystem::path& project_path) noexcept;
    [[nodiscard]] core::Status save() noexcept;

    [[nodiscard]] const ProjectManifest& manifest() const noexcept { return manifest_; }
    [[nodiscard]] const std::filesystem::path& project_path() const noexcept
    {
        return project_path_;
    }
    [[nodiscard]] const std::filesystem::path& scene_path() const noexcept { return scene_path_; }
    [[nodiscard]] scene::Scene& scene() noexcept { return scene_; }
    [[nodiscard]] const scene::Scene& scene() const noexcept { return scene_; }
    [[nodiscard]] scene::Entity selected_entity() const noexcept { return selected_entity_; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] std::string_view message() const noexcept { return message_; }

    [[nodiscard]] core::Status select(scene::Entity entity) noexcept;
    [[nodiscard]] core::Status move_selected(core::f32 x, core::f32 y, core::f32 z) noexcept;
    [[nodiscard]] core::Status reset_selected() noexcept;

private:
    [[nodiscard]] core::Status load_scene_file(const std::filesystem::path& scene_path,
                                                scene::Scene& output) const noexcept;
    [[nodiscard]] core::Status write_scene_file(const std::filesystem::path& scene_path) noexcept;
    [[nodiscard]] core::Status validate_bootstrap_scene() const noexcept;
    void set_message(const char* message) noexcept;

    ProjectManifest manifest_{};
    std::filesystem::path project_path_{};
    std::filesystem::path scene_path_{};
    scene::Scene scene_{};
    scene::Entity selected_entity_{};
    scene::TransformComponent selected_initial_transform_{};
    bool has_selected_initial_transform_ = false;
    bool dirty_ = false;
    std::string message_{};
};

[[nodiscard]] core::Status parse_project_manifest(std::string_view json,
                                                   ProjectManifest& manifest) noexcept;
[[nodiscard]] std::string serialize_project_manifest(const ProjectManifest& manifest);

[[nodiscard]] core::Status create_bootstrap_project(const std::filesystem::path& directory) noexcept;

} // namespace gameengine::editor
