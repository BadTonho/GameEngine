#include "engine/editor/project.hpp"
#include "engine/scene/scene_serialization.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace {

bool expect(bool value) noexcept
{
    return value;
}

constexpr std::size_t align16(std::size_t value) noexcept
{
    return (value + 15U) & ~std::size_t{15U};
}

constexpr std::uint32_t fourcc(char a, char b, char c, char d) noexcept
{
    return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8U) |
           (static_cast<std::uint32_t>(c) << 16U) | (static_cast<std::uint32_t>(d) << 24U);
}

std::uint64_t hash_bytes(std::span<const std::byte> bytes,
                         std::uint64_t hash = 0xcbf29ce484222325ULL) noexcept
{
    for (const std::byte byte : bytes) {
        hash ^= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(byte));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

void put_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) noexcept
{
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[offset + index] =
            std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void put_u64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value) noexcept
{
    for (std::size_t index = 0; index < 8U; ++index) {
        bytes[offset + index] =
            std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4U);
    put_u32(bytes, offset, value);
}

void append_u64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 8U);
    put_u64(bytes, offset, value);
}

void append_f32(std::vector<std::byte>& bytes, float value)
{
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

struct Chunk final {
    std::uint32_t kind = 0U;
    std::vector<std::byte> data{};
};

std::vector<std::byte> build_asset(std::array<std::byte, 4> magic, const std::vector<Chunk>& chunks)
{
    std::size_t cursor = align16(64U + chunks.size() * 40U);
    std::vector<std::size_t> offsets;
    offsets.reserve(chunks.size());
    std::vector<std::byte> payload;
    for (const Chunk& chunk : chunks) {
        cursor = align16(cursor);
        offsets.push_back(cursor);
        payload.insert(payload.end(), chunk.data.begin(), chunk.data.end());
        cursor += chunk.data.size();
    }
    const std::uint64_t content_hash = hash_bytes(payload);
    const std::uint64_t asset_id = hash_bytes(payload, hash_bytes(magic));
    std::vector<std::byte> output(cursor);
    for (std::size_t i = 0; i < 4U; ++i) {
        output[i] = magic[i];
    }
    put_u32(output, 4U, 1U | (64U << 16U));
    put_u32(output, 8U, 1U);
    put_u64(output, 12U, output.size());
    put_u64(output, 20U, asset_id);
    put_u64(output, 28U, content_hash);
    put_u32(output, 36U, static_cast<std::uint32_t>(chunks.size()));
    put_u32(output, 40U, 40U);
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        const std::size_t entry = 64U + index * 40U;
        const std::size_t offset = offsets[index];
        put_u32(output, entry, chunks[index].kind);
        put_u64(output, entry + 8U, offset);
        put_u64(output, entry + 16U, chunks[index].data.size());
        put_u64(output, entry + 24U, chunks[index].data.size());
        put_u32(output, entry + 32U, 16U);
        for (std::size_t b = 0; b < chunks[index].data.size(); ++b) {
            output[offset + b] = chunks[index].data[b];
        }
    }
    return output;
}

bool write_binary(const std::filesystem::path& path, std::span<const std::byte> bytes) noexcept
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    output.flush();
    return static_cast<bool>(output);
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

    // Project with prepared .gemesh/.getex/.gemat/.gescene assets loads, edits, saves, and rejects missing textures.
    const std::filesystem::path prepared_dir = directory / "prepared";
    std::vector<std::byte> tex_header;
    for (const std::uint32_t v : {2U, 2U, 1U, 1U, 16U}) {
        append_u32(tex_header, v);
    }
    const auto tex_bytes = build_asset(
        {std::byte{'G'}, std::byte{'T'}, std::byte{'E'}, std::byte{'X'}},
        {{fourcc('T', 'X', 'H', 'D'), tex_header},
         {fourcc('T', 'X', 'D', 'T'), std::vector<std::byte>(16U, std::byte{255})}});
    gameengine::assets::TextureView tex_view{};
    if (!expect(gameengine::assets::read_texture(tex_bytes, tex_view).ok())) {
        std::filesystem::remove_all(directory, error);
        return 10;
    }

    std::vector<std::byte> mat_data;
    for (const float v : {0.9F, 0.2F, 0.2F, 1.0F, 0.1F, 0.4F}) {
        append_f32(mat_data, v);
    }
    append_u64(mat_data, tex_view.asset.asset_id);
    append_u64(mat_data, 0U);
    append_u64(mat_data, 0U);
    mat_data.resize(64U, std::byte{0});
    const auto mat_bytes = build_asset(
        {std::byte{'G'}, std::byte{'M'}, std::byte{'A'}, std::byte{'T'}},
        {{fourcc('M', 'T', 'H', 'D'), mat_data}});
    gameengine::assets::MaterialView mat_view{};
    if (!expect(gameengine::assets::read_material(mat_bytes, mat_view).ok())) {
        std::filesystem::remove_all(directory, error);
        return 11;
    }

    std::vector<std::byte> mesh_header;
    for (const std::uint32_t v : {1U, 3U, 32U, 16U, 3U, 1U}) {
        append_u32(mesh_header, v);
    }
    std::vector<std::byte> mesh_verts;
    for (const float v : {
             -0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
              0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F,
              0.0F,  0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.5F, 0.0F}) {
        append_f32(mesh_verts, v);
    }
    std::vector<std::byte> mesh_indices;
    for (const std::uint16_t idx : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}}) {
        mesh_indices.push_back(std::byte{static_cast<unsigned char>(idx)});
        mesh_indices.push_back(std::byte{0});
    }
    std::vector<std::byte> mesh_submeshes;
    for (const std::uint32_t v : {0U, 3U, 0U, 0U}) {
        append_u32(mesh_submeshes, v);
    }
    const auto mesh_bytes = build_asset(
        {std::byte{'G'}, std::byte{'M'}, std::byte{'S'}, std::byte{'H'}},
        {{fourcc('M', 'S', 'H', 'D'), mesh_header},
         {fourcc('V', 'E', 'R', 'T'), mesh_verts},
         {fourcc('I', 'N', 'D', 'X'), mesh_indices},
         {fourcc('S', 'U', 'B', 'M'), mesh_submeshes}});
    gameengine::assets::MeshView mesh_view{};
    if (!expect(gameengine::assets::read_mesh(mesh_bytes, mesh_view).ok())) {
        std::filesystem::remove_all(directory, error);
        return 12;
    }

    gameengine::scene::Scene custom_scene;
    const auto entity = custom_scene.create_entity();
    if (!expect(custom_scene.add_transform(entity).ok()) ||
        !expect(custom_scene
                    .add_mesh_renderer(entity, {mesh_view.asset.asset_id, mat_view.asset.asset_id})
                    .ok()) ||
        !expect(gameengine::scene::ensure_default_camera_and_light(custom_scene).ok())) {
        std::filesystem::remove_all(directory, error);
        return 13;
    }
    std::vector<std::byte> custom_scene_bytes;
    if (!expect(gameengine::scene::serialize_scene(custom_scene, custom_scene_bytes).ok())) {
        std::filesystem::remove_all(directory, error);
        return 14;
    }

    const std::filesystem::path tex_file = prepared_dir / "assets" / "albedo.getex";
    const std::filesystem::path mat_file = prepared_dir / "assets" / "red.gemat";
    const std::filesystem::path mesh_file = prepared_dir / "assets" / "tri.gemesh";
    const std::filesystem::path scene_file = prepared_dir / "scenes" / "main.gescene";
    const std::filesystem::path proj_file = prepared_dir / "project.geproject";
    if (!expect(write_binary(tex_file, tex_bytes)) ||
        !expect(write_binary(mat_file, mat_bytes)) ||
        !expect(write_binary(mesh_file, mesh_bytes)) ||
        !expect(write_binary(scene_file, custom_scene_bytes))) {
        std::filesystem::remove_all(directory, error);
        return 15;
    }
    {
        std::ofstream proj_out(proj_file, std::ios::binary | std::ios::trunc);
        proj_out << gameengine::editor::serialize_project_manifest(ProjectManifest{
            .name = "PreparedDemo",
            .scene = "scenes/main.gescene",
        });
        if (!expect(static_cast<bool>(proj_out))) {
            std::filesystem::remove_all(directory, error);
            return 16;
        }
    }

    ProjectSession prepared_session;
    if (!expect(prepared_session.load(proj_file).ok()) ||
        !expect(prepared_session.prepared_assets().meshes().size() == 1U) ||
        !expect(prepared_session.prepared_assets().textures().size() == 1U) ||
        !expect(prepared_session.prepared_assets().materials().size() == 1U) ||
        !expect(prepared_session.selected_entity() == entity) ||
        !expect(prepared_session.move_selected(0.5F, 0.25F, 0.0F).ok()) ||
        !expect(prepared_session.save().ok())) {
        std::filesystem::remove_all(directory, error);
        return 17;
    }

    std::filesystem::remove(tex_file, error);
    if (!expect(prepared_session.load(proj_file).code ==
                gameengine::core::ErrorCode::invalid_argument) ||
        !expect(prepared_session.prepared_assets().textures().size() == 1U)) {
        std::filesystem::remove_all(directory, error);
        return 18;
    }

    std::filesystem::remove_all(directory, error);
    return 0;
}
