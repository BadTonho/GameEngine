#include "engine/editor/project.hpp"

#include "engine/scene/bootstrap_cube.hpp"
#include "engine/scene/scene_serialization.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>
#include <utility>

namespace gameengine::editor {

namespace {

[[nodiscard]] core::Status invalid() noexcept
{
    return core::Status{core::ErrorCode::invalid_argument};
}

class JsonParser final {
public:
    explicit JsonParser(std::string_view input) noexcept : input_(input) {}

    [[nodiscard]] bool parse_manifest(ProjectManifest& manifest) noexcept
    {
        skip_space();
        if (!consume('{')) {
            return false;
        }
        bool has_format = false;
        bool has_name = false;
        bool has_scene = false;
        bool has_quality = false;
        bool has_window = false;
        while (true) {
            skip_space();
            if (consume('}')) {
                break;
            }
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            skip_space();
            if (!consume(':')) {
                return false;
            }
            if (key == "format") {
                if (has_format) {
                    return false;
                }
                core::u32 value = 0;
                if (!parse_u32(value) || value != ProjectManifest::format_version) {
                    return false;
                }
                has_format = true;
            } else if (key == "name") {
                if (has_name) {
                    return false;
                }
                skip_space();
                if (!parse_string(manifest.name) || manifest.name.empty()) {
                    return false;
                }
                has_name = true;
            } else if (key == "scene") {
                if (has_scene) {
                    return false;
                }
                skip_space();
                if (!parse_string(manifest.scene) || !valid_relative_path(manifest.scene)) {
                    return false;
                }
                has_scene = true;
            } else if (key == "renderer_quality") {
                if (has_quality) {
                    return false;
                }
                skip_space();
                std::string value;
                if (!parse_string(value) ||
                    !renderer::quality::parse(value, manifest.renderer_quality)) {
                    return false;
                }
                has_quality = true;
            } else if (key == "window") {
                if (has_window || !parse_window(manifest)) {
                    return false;
                }
                has_window = true;
            } else {
                return false;
            }
            skip_space();
            if (consume('}')) {
                break;
            }
            if (!consume(',')) {
                return false;
            }
        }
        skip_space();
        return has_format && has_name && has_scene && position_ == input_.size();
    }

private:
    [[nodiscard]] static bool valid_relative_path(std::string_view value) noexcept
    {
        if (value.empty() || value.front() == '/' || value.front() == '\\' ||
            (value.size() >= 2U && std::isalpha(static_cast<unsigned char>(value[0])) != 0 &&
             value[1] == ':')) {
            return false;
        }
        std::size_t start = 0;
        while (start < value.size()) {
            const std::size_t separator = value.find_first_of("/\\", start);
            const std::size_t end = separator == std::string_view::npos ? value.size() : separator;
            const std::string_view component = value.substr(start, end - start);
            if (component.empty() || component == "." || component == "..") {
                return false;
            }
            if (separator == std::string_view::npos) {
                break;
            }
            start = separator + 1U;
        }
        return true;
    }

    [[nodiscard]] bool parse_window(ProjectManifest& manifest) noexcept
    {
        skip_space();
        if (!consume('{')) {
            return false;
        }
        bool has_width = false;
        bool has_height = false;
        while (true) {
            skip_space();
            if (consume('}')) {
                break;
            }
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            skip_space();
            if (!consume(':')) {
                return false;
            }
            core::u32 value = 0;
            if (!parse_u32(value) || value == 0U || value > 16'384U) {
                return false;
            }
            if (key == "width") {
                if (has_width) {
                    return false;
                }
                manifest.window_width = value;
                has_width = true;
            } else if (key == "height") {
                if (has_height) {
                    return false;
                }
                manifest.window_height = value;
                has_height = true;
            } else {
                return false;
            }
            skip_space();
            if (consume('}')) {
                break;
            }
            if (!consume(',')) {
                return false;
            }
        }
        return has_width && has_height;
    }

    [[nodiscard]] bool parse_string(std::string& value) noexcept
    {
        value.clear();
        if (!consume('"')) {
            return false;
        }
        while (position_ < input_.size()) {
            const char character = input_[position_++];
            if (character == '"') {
                return true;
            }
            if (character == '\\') {
                if (position_ >= input_.size()) {
                    return false;
                }
                const char escaped = input_[position_++];
                switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    value.push_back(escaped);
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                default:
                    return false;
                }
            } else {
                if (static_cast<unsigned char>(character) < 0x20U) {
                    return false;
                }
                value.push_back(character);
            }
        }
        return false;
    }

    [[nodiscard]] bool parse_u32(core::u32& value) noexcept
    {
        skip_space();
        if (position_ >= input_.size() || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            return false;
        }
        core::u64 result = 0;
        while (position_ < input_.size() &&
               std::isdigit(static_cast<unsigned char>(input_[position_])) != 0) {
            result = result * 10U + static_cast<core::u64>(input_[position_] - '0');
            if (result > std::numeric_limits<core::u32>::max()) {
                return false;
            }
            ++position_;
        }
        value = static_cast<core::u32>(result);
        return true;
    }

    void skip_space() noexcept
    {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
            ++position_;
        }
    }

    [[nodiscard]] bool consume(char expected) noexcept
    {
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

[[nodiscard]] bool read_text_file(const std::filesystem::path& path, std::string& output) noexcept
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    output.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    return file.good() || file.eof();
}

[[nodiscard]] bool read_binary_file(const std::filesystem::path& path,
                                    std::vector<std::byte>& output) noexcept
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    output.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(output.data()), size);
    return file.good() || file.eof();
}

[[nodiscard]] bool valid_scene_path(std::string_view value) noexcept
{
    if (value.empty() || value.front() == '/' || value.front() == '\\' ||
        (value.size() >= 2U && std::isalpha(static_cast<unsigned char>(value[0])) != 0 &&
         value[1] == ':')) {
        return false;
    }
    std::size_t start = 0;
    while (start < value.size()) {
        const std::size_t separator = value.find_first_of("/\\", start);
        const std::size_t end = separator == std::string_view::npos ? value.size() : separator;
        const std::string_view component = value.substr(start, end - start);
        if (component.empty() || component == "." || component == "..") {
            return false;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1U;
    }
    return true;
}

[[nodiscard]] std::filesystem::path normalize_scene_path(const std::filesystem::path& project,
                                                          std::string_view scene) noexcept
{
    std::string normalized{scene};
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return project.parent_path() / std::filesystem::path{normalized};
}

[[nodiscard]] std::string escape_json_string(std::string_view value)
{
    std::string output;
    output.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output.push_back(character);
            break;
        }
    }
    return output;
}

[[nodiscard]] core::Status write_file(const std::filesystem::path& path,
                                      std::span<const std::byte> data) noexcept
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return invalid();
    }
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good() ? core::Status{} : invalid();
}

[[nodiscard]] core::Status replace_file(const std::filesystem::path& temporary,
                                        const std::filesystem::path& target) noexcept
{
#if defined(_WIN32)
    if (MoveFileExW(temporary.c_str(),
                    target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        return invalid();
    }
    return core::Status{};
#else
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    return error ? invalid() : core::Status{};
#endif
}

[[nodiscard]] core::Status populate_bootstrap_scene(scene::Scene& scene) noexcept
{
    return scene::create_bootstrap_scene(scene);
}

} // namespace

core::Status parse_project_manifest(std::string_view json, ProjectManifest& manifest) noexcept
{
    ProjectManifest parsed{};
    JsonParser parser{json};
    if (!parser.parse_manifest(parsed)) {
        return invalid();
    }
    manifest = std::move(parsed);
    return core::Status{};
}

std::string serialize_project_manifest(const ProjectManifest& manifest)
{
    const char* quality = renderer::quality::name(manifest.renderer_quality).data();
    return "{\n"
           "  \"format\": 1,\n"
           "  \"name\": \"" + escape_json_string(manifest.name) + "\",\n"
           "  \"scene\": \"" + escape_json_string(manifest.scene) + "\",\n"
           "  \"renderer_quality\": \"" + quality + "\",\n"
           "  \"window\": {\n"
           "    \"width\": " + std::to_string(manifest.window_width) + ",\n"
           "    \"height\": " + std::to_string(manifest.window_height) + "\n"
           "  }\n"
           "}\n";
}

core::Status ProjectSession::create_new(const std::filesystem::path& directory) noexcept
{
    if (directory.empty()) {
        return invalid();
    }
    std::error_code error;
    std::filesystem::create_directories(directory / "scenes", error);
    if (error) {
        return invalid();
    }
    project_path_ = directory / "gameengine.geproject";
    scene_path_ = directory / "scenes" / "main.gescene";
    manifest_ = {};
    scene::Scene generated;
    if (!populate_bootstrap_scene(generated)) {
        return invalid();
    }
    scene_.swap(generated);
    std::vector<std::byte> scene_bytes;
    if (!scene::serialize_scene(scene_, scene_bytes) ||
        !write_file(scene_path_, scene_bytes)) {
        return invalid();
    }
    const std::string manifest_text = serialize_project_manifest(manifest_);
    const std::span<const std::byte> manifest_bytes{
        reinterpret_cast<const std::byte*>(manifest_text.data()), manifest_text.size()};
    if (!write_file(project_path_, manifest_bytes)) {
        return invalid();
    }
    selected_entity_ = scene_.entities().empty() ? scene::invalid_entity : scene_.entities().front();
    has_selected_initial_transform_ = false;
    if (selected_entity_.valid()) {
        const scene::TransformComponent* transform = scene_.transform(selected_entity_);
        if (transform != nullptr) {
            selected_initial_transform_ = *transform;
            has_selected_initial_transform_ = true;
        }
    }
    dirty_ = false;
    set_message("created procedural project");
    return core::Status{};
}

core::Status ProjectSession::load(const std::filesystem::path& project_path) noexcept
{
    std::string json;
    ProjectManifest manifest;
    if (!read_text_file(project_path, json) || !parse_project_manifest(json, manifest)) {
        return invalid();
    }
    const std::filesystem::path scene_path = normalize_scene_path(project_path, manifest.scene);
    scene::Scene loaded;
    if (!load_scene_file(scene_path, loaded)) {
        return invalid();
    }
    scene_.swap(loaded);
    manifest_ = std::move(manifest);
    project_path_ = project_path;
    scene_path_ = scene_path;
    selected_entity_ = scene::invalid_entity;
    for (const scene::Entity entity : scene_.entities()) {
        if (scene_.mesh_renderer(entity) != nullptr) {
            selected_entity_ = entity;
            break;
        }
    }
    if (selected_entity_.valid()) {
        const scene::TransformComponent* transform = scene_.transform(selected_entity_);
        if (transform != nullptr) {
            selected_initial_transform_ = *transform;
            has_selected_initial_transform_ = true;
        }
    }
    dirty_ = false;
    set_message("project loaded");
    return core::Status{};
}

core::Status ProjectSession::save() noexcept
{
    if (project_path_.empty() || scene_path_.empty() || !scene_.validate()) {
        return invalid();
    }
    if (!write_scene_file(scene_path_)) {
        return invalid();
    }
    const std::string text = serialize_project_manifest(manifest_);
    const std::span<const std::byte> bytes{
        reinterpret_cast<const std::byte*>(text.data()), text.size()};
    const std::filesystem::path temporary = project_path_.string() + ".tmp";
    if (!write_file(temporary, bytes) || !replace_file(temporary, project_path_)) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        return invalid();
    }
    dirty_ = false;
    set_message("project saved");
    return core::Status{};
}

core::Status ProjectSession::select(scene::Entity entity) noexcept
{
    if (!scene_.is_alive(entity) || scene_.transform(entity) == nullptr) {
        return invalid();
    }
    selected_entity_ = entity;
    selected_initial_transform_ = *scene_.transform(entity);
    has_selected_initial_transform_ = true;
    return core::Status{};
}

core::Status ProjectSession::move_selected(core::f32 x, core::f32 y, core::f32 z) noexcept
{
    scene::TransformComponent* transform = scene_.transform(selected_entity_);
    if (transform == nullptr || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        return invalid();
    }
    transform->local_position.x += x;
    transform->local_position.y += y;
    transform->local_position.z += z;
    if (!scene_.update_transforms()) {
        return invalid();
    }
    dirty_ = true;
    set_message("scene modified");
    return core::Status{};
}

core::Status ProjectSession::reset_selected() noexcept
{
    scene::TransformComponent* transform = scene_.transform(selected_entity_);
    if (transform == nullptr || !has_selected_initial_transform_) {
        return invalid();
    }
    *transform = selected_initial_transform_;
    if (!scene_.update_transforms()) {
        return invalid();
    }
    dirty_ = true;
    set_message("transform restored");
    return core::Status{};
}

core::Status ProjectSession::load_scene_file(const std::filesystem::path& scene_path,
                                             scene::Scene& output) const noexcept
{
    std::vector<std::byte> bytes;
    if (!read_binary_file(scene_path, bytes) || !scene::deserialize_scene(bytes, output) ||
        !output.validate()) {
        return invalid();
    }
    for (const scene::Entity entity : output.entities()) {
        const scene::MeshRendererComponent* mesh = output.mesh_renderer(entity);
        if (mesh != nullptr &&
            (mesh->mesh_id != scene::bootstrap_mesh_id ||
             mesh->material_id != scene::bootstrap_material_id)) {
            return core::Status{core::ErrorCode::unsupported_platform};
        }
    }
    return output.active_camera().valid() ? core::Status{} : invalid();
}

core::Status ProjectSession::write_scene_file(const std::filesystem::path& scene_path) noexcept
{
    std::vector<std::byte> bytes;
    if (!scene::serialize_scene(scene_, bytes)) {
        return invalid();
    }
    const std::filesystem::path temporary = scene_path.string() + ".tmp";
    if (!write_file(temporary, bytes) || !replace_file(temporary, scene_path)) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        return invalid();
    }
    return core::Status{};
}

core::Status ProjectSession::validate_bootstrap_scene() const noexcept
{
    return scene_.validate();
}

void ProjectSession::set_message(const char* message) noexcept
{
    message_ = message == nullptr ? "" : message;
}

core::Status create_bootstrap_project(const std::filesystem::path& directory) noexcept
{
    ProjectSession session;
    return session.create_new(directory);
}

} // namespace gameengine::editor
