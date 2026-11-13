#include "engine/assets/prepared_assets.hpp"
#include "engine/renderer/scene_bridge.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/scene_serialization.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace {

using gameengine::core::u32;
using gameengine::core::u64;

struct Chunk final {
    u32 kind = 0;
    std::vector<std::byte> data{};
};

constexpr std::size_t align16(std::size_t value) noexcept
{
    return (value + 15U) & ~std::size_t{15U};
}

constexpr u32 fourcc(char a, char b, char c, char d) noexcept
{
    return static_cast<u32>(a) | (static_cast<u32>(b) << 8U) |
           (static_cast<u32>(c) << 16U) | (static_cast<u32>(d) << 24U);
}

u64 hash_bytes(std::span<const std::byte> bytes, u64 hash = 0xcbf29ce484222325ULL) noexcept
{
    for (const std::byte byte : bytes) {
        hash ^= static_cast<u64>(std::to_integer<std::uint8_t>(byte));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

void put_u32(std::vector<std::byte>& bytes, std::size_t offset, u32 value) noexcept
{
    bytes[offset] = std::byte{static_cast<unsigned char>(value & 0xffU)};
    bytes[offset + 1] = std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)};
    bytes[offset + 2] = std::byte{static_cast<unsigned char>((value >> 16U) & 0xffU)};
    bytes[offset + 3] = std::byte{static_cast<unsigned char>((value >> 24U) & 0xffU)};
}

void put_u64(std::vector<std::byte>& bytes, std::size_t offset, u64 value) noexcept
{
    for (std::size_t index = 0; index < sizeof(u64); ++index) {
        bytes[offset + index] =
            std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void append_u16(std::vector<std::byte>& bytes, std::uint16_t value)
{
    bytes.push_back(std::byte{static_cast<unsigned char>(value & 0xffU)});
    bytes.push_back(std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)});
}

void append_u32(std::vector<std::byte>& bytes, u32 value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4U);
    put_u32(bytes, offset, value);
}

void append_u64(std::vector<std::byte>& bytes, u64 value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 8U);
    put_u64(bytes, offset, value);
}

void append_float(std::vector<std::byte>& bytes, float value)
{
    append_u32(bytes, std::bit_cast<u32>(value));
}

std::vector<std::byte> build_asset(std::array<std::byte, 4> magic,
                                   const std::vector<Chunk>& chunks,
                                   u64 explicit_asset_id = 0U)
{
    const std::size_t table_end = 64U + chunks.size() * 40U;
    std::size_t cursor = align16(table_end);
    std::vector<std::size_t> offsets;
    offsets.reserve(chunks.size());
    std::vector<std::byte> payload;
    for (const Chunk& chunk : chunks) {
        cursor = align16(cursor);
        offsets.push_back(cursor);
        payload.insert(payload.end(), chunk.data.begin(), chunk.data.end());
        cursor += chunk.data.size();
    }

    const u64 content_hash = hash_bytes(payload);
    const u64 asset_id =
        explicit_asset_id != 0U ? explicit_asset_id : hash_bytes(payload, hash_bytes(magic));
    std::vector<std::byte> output(cursor);
    std::copy(magic.begin(), magic.end(), output.begin());
    put_u32(output, 4U, 1U | (64U << 16U));
    put_u32(output, 8U, 1U);
    put_u64(output, 12U, output.size());
    put_u64(output, 20U, asset_id);
    put_u64(output, 28U, content_hash);
    put_u32(output, 36U, static_cast<u32>(chunks.size()));
    put_u32(output, 40U, 40U);
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        const std::size_t entry = 64U + index * 40U;
        const std::size_t offset = offsets[index];
        put_u32(output, entry, chunks[index].kind);
        put_u64(output, entry + 8U, offset);
        put_u64(output, entry + 16U, chunks[index].data.size());
        put_u64(output, entry + 24U, chunks[index].data.size());
        put_u32(output, entry + 32U, 16U);
        std::copy(chunks[index].data.begin(),
                  chunks[index].data.end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    return output;
}

std::vector<std::byte> make_texture_bytes(u32 width,
                                          u32 height,
                                          unsigned char r,
                                          unsigned char g,
                                          unsigned char b,
                                          unsigned char a,
                                          u64 explicit_asset_id = 0U)
{
    std::vector<std::byte> header;
    append_u32(header, width);
    append_u32(header, height);
    append_u32(header, 1U);
    append_u32(header, 1U);
    append_u32(header, width * height * 4U);

    std::vector<std::byte> pixels;
    pixels.reserve(static_cast<std::size_t>(width) * height * 4U);
    for (u32 i = 0; i < width * height; ++i) {
        pixels.push_back(std::byte{r});
        pixels.push_back(std::byte{g});
        pixels.push_back(std::byte{b});
        pixels.push_back(std::byte{a});
    }
    return build_asset(
        {std::byte{'G'}, std::byte{'T'}, std::byte{'E'}, std::byte{'X'}},
        {{fourcc('T', 'X', 'H', 'D'), header}, {fourcc('T', 'X', 'D', 'T'), pixels}},
        explicit_asset_id);
}

std::vector<std::byte> make_material_bytes(u64 albedo_texture,
                                           u64 normal_texture,
                                           u64 orm_texture,
                                           u64 explicit_asset_id = 0U)
{
    std::vector<std::byte> data;
    for (const float value : {0.8F, 0.4F, 0.2F, 1.0F, 0.25F, 0.6F}) {
        append_float(data, value);
    }
    append_u64(data, albedo_texture);
    append_u64(data, normal_texture);
    append_u64(data, orm_texture);
    append_u64(data, 0U);
    data.resize(64U, std::byte{0});
    return build_asset(
        {std::byte{'G'}, std::byte{'M'}, std::byte{'A'}, std::byte{'T'}},
        {{fourcc('M', 'T', 'H', 'D'), data}},
        explicit_asset_id);
}

std::vector<std::byte> make_mesh_bytes(bool use_u32_indices, u64 explicit_asset_id = 0U)
{
    std::vector<std::byte> vertices;
    for (const float value : {
             -0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
              0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F,
              0.5F,  0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F,
             -0.5F,  0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}) {
        append_float(vertices, value);
    }

    std::vector<std::byte> indices;
    if (use_u32_indices) {
        for (const u32 index : {0U, 1U, 2U, 2U, 3U, 0U}) {
            append_u32(indices, index);
        }
    } else {
        for (const std::uint16_t index : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}}) {
            append_u16(indices, index);
        }
    }

    std::vector<std::byte> submeshes;
    if (use_u32_indices) {
        for (const u32 value : {0U, 3U, 0U, 0U, 3U, 3U, 1U, 0U}) {
            append_u32(submeshes, value);
        }
    } else {
        for (const u32 value : {0U, 3U, 0U, 0U}) {
            append_u32(submeshes, value);
        }
    }

    std::vector<std::byte> header;
    append_u32(header, 1U);
    append_u32(header, 4U);
    append_u32(header, 32U);
    append_u32(header, use_u32_indices ? 32U : 16U);
    append_u32(header, use_u32_indices ? 6U : 3U);
    append_u32(header, use_u32_indices ? 2U : 1U);

    return build_asset(
        {std::byte{'G'}, std::byte{'M'}, std::byte{'S'}, std::byte{'H'}},
        {{fourcc('M', 'S', 'H', 'D'), header},
         {fourcc('V', 'E', 'R', 'T'), vertices},
         {fourcc('I', 'N', 'D', 'X'), indices},
         {fourcc('S', 'U', 'B', 'M'), submeshes}},
        explicit_asset_id);
}

std::vector<std::byte> make_legacy_scene_bytes(u64 mesh_id, u64 material_id)
{
    std::vector<std::byte> header;
    append_u32(header, 1U);
    append_u32(header, 0U);
    std::vector<std::byte> instances;
    for (std::size_t index = 0; index < 16U; ++index) {
        append_float(
            instances,
            (index == 0U || index == 5U || index == 10U || index == 15U) ? 1.0F : 0.0F);
    }
    append_u64(instances, mesh_id);
    append_u64(instances, material_id);
    return build_asset(
        {std::byte{'G'}, std::byte{'S'}, std::byte{'C'}, std::byte{'N'}},
        {{fourcc('S', 'C', 'H', 'D'), header},
         {fourcc('I', 'N', 'S', 'T'), instances}});
}

bool write_bytes(const std::filesystem::path& path, std::span<const std::byte> bytes) noexcept
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
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    output.flush();
    return static_cast<bool>(output);
}

} // namespace

int main()
{
    using namespace gameengine;
    using namespace gameengine::assets;

    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) / "gameengine_prepared_assets_test";
    if (error) {
        return 1;
    }
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "nested", error);
    if (error) {
        return 2;
    }

    const auto cleanup = [&]() noexcept {
        std::error_code remove_error;
        std::filesystem::remove_all(root, remove_error);
    };

    const auto texture_bytes = make_texture_bytes(2U, 2U, 200U, 100U, 50U, 255U);
    TextureView tex_view{};
    if (!read_texture(texture_bytes, tex_view).ok()) {
        cleanup();
        return 3;
    }
    const u64 tex_id = tex_view.asset.asset_id;

    const auto material_bytes = make_material_bytes(tex_id, 0U, 0U);
    MaterialView mat_view{};
    if (!read_material(material_bytes, mat_view).ok()) {
        cleanup();
        return 4;
    }
    const u64 mat_id = mat_view.asset.asset_id;

    const auto mesh_u16_bytes = make_mesh_bytes(false);
    const auto mesh_u32_bytes = make_mesh_bytes(true);
    MeshView mesh_u16_view{};
    MeshView mesh_u32_view{};
    if (!read_mesh(mesh_u16_bytes, mesh_u16_view).ok() ||
        !read_mesh(mesh_u32_bytes, mesh_u32_view).ok()) {
        cleanup();
        return 5;
    }

    const std::filesystem::path tex_path = root / "nested" / "albedo.getex";
    const std::filesystem::path mat_path = root / "nested" / "lit.gemat";
    const std::filesystem::path mesh16_path = root / "tri.GEMESH";
    const std::filesystem::path mesh32_path = root / "nested" / "quad.gemesh";
    const std::filesystem::path note_path = root / "readme.txt";
    if (!write_bytes(tex_path, texture_bytes) || !write_bytes(mat_path, material_bytes) ||
        !write_bytes(mesh16_path, mesh_u16_bytes) || !write_bytes(mesh32_path, mesh_u32_bytes)) {
        cleanup();
        return 6;
    }
    {
        std::ofstream note(note_path, std::ios::binary | std::ios::trunc);
        note << "ignore non-asset file";
        if (!note) {
            cleanup();
            return 7;
        }
    }

    std::vector<std::byte> read_back;
    if (read_binary_asset_file({}, read_back).code != core::ErrorCode::invalid_argument ||
        read_binary_asset_file(tex_path, read_back, 0U).code != core::ErrorCode::invalid_argument ||
        read_binary_asset_file(root / "missing.getex", read_back).code !=
            core::ErrorCode::invalid_argument ||
        read_binary_asset_file(tex_path, read_back, 4U).code !=
            core::ErrorCode::invalid_argument ||
        !read_binary_asset_file(tex_path, read_back).ok() || read_back != texture_bytes) {
        cleanup();
        return 8;
    }

    PreparedAssetBundle bundle;
    if (bundle.load_directory({}).code != core::ErrorCode::invalid_argument ||
        bundle.load_directory(root / "does_not_exist").code != core::ErrorCode::invalid_argument ||
        bundle.load_file(note_path).code != core::ErrorCode::invalid_argument) {
        cleanup();
        return 9;
    }
    if (!bundle.load_directory(root).ok() || bundle.total_count() != 4U ||
        bundle.meshes().size() != 2U || bundle.textures().size() != 1U ||
        bundle.materials().size() != 1U || bundle.memory_usage_bytes() == 0U) {
        cleanup();
        return 10;
    }

    if (bundle.find_mesh(0U) != nullptr || bundle.find_texture(0U) != nullptr ||
        bundle.find_material(0U) != nullptr ||
        bundle.find_mesh(mesh_u16_view.asset.asset_id) == nullptr ||
        bundle.find_mesh(mesh_u32_view.asset.asset_id) == nullptr ||
        bundle.find_texture(tex_id) == nullptr || bundle.find_material(mat_id) == nullptr ||
        bundle.find_mesh(mesh_u32_view.asset.asset_id)->submeshes.size() != 2U) {
        cleanup();
        return 11;
    }

    // Idempotent reload of identical file must succeed without duplicating entries.
    if (!bundle.load_file(mesh16_path).ok() || !bundle.load_file(tex_path).ok() ||
        !bundle.load_file(mat_path).ok() || bundle.total_count() != 4U) {
        cleanup();
        return 12;
    }

    // Same asset ID with different content hash must fail.
    const std::filesystem::path conflict_tex_path = root / "conflict.getex";
    const auto conflict_tex_bytes = make_texture_bytes(2U, 2U, 10U, 20U, 30U, 255U, tex_id);
    if (!write_bytes(conflict_tex_path, conflict_tex_bytes) ||
        bundle.load_file(conflict_tex_path).code != core::ErrorCode::invalid_argument) {
        cleanup();
        return 13;
    }

    // Scene dependency validation: valid extended scene and flat INST scene.
    scene::Scene valid_scene;
    const scene::Entity entity = valid_scene.create_entity();
    const scene::Entity builtin_entity = valid_scene.create_entity();
    if (!valid_scene.add_transform(entity).ok() ||
        !valid_scene.add_mesh_renderer(entity, {mesh_u32_view.asset.asset_id, mat_id}).ok() ||
        !valid_scene.add_transform(builtin_entity).ok() ||
        !valid_scene
             .add_mesh_renderer(builtin_entity,
                                {scene::bootstrap_mesh_id, scene::bootstrap_material_id})
             .ok() ||
        !scene::ensure_default_camera_and_light(valid_scene).ok()) {
        cleanup();
        return 14;
    }
    std::vector<std::byte> valid_scene_bytes;
    SceneView valid_scene_view{};
    if (!scene::serialize_scene(valid_scene, valid_scene_bytes).ok() ||
        !read_scene(valid_scene_bytes, valid_scene_view).ok()) {
        cleanup();
        return 15;
    }
    const std::array builtin_meshes{scene::bootstrap_mesh_id};
    const std::array builtin_materials{scene::bootstrap_material_id};
    if (!bundle.validate_scene_dependencies(valid_scene_view, builtin_meshes, builtin_materials)
             .ok()) {
        cleanup();
        return 16;
    }

    const auto flat_scene_bytes = make_legacy_scene_bytes(mesh_u16_view.asset.asset_id, mat_id);
    SceneView flat_scene_view{};
    if (!read_scene(flat_scene_bytes, flat_scene_view).ok() || flat_scene_view.extended ||
        !bundle.validate_scene_dependencies(flat_scene_view, builtin_meshes, builtin_materials)
             .ok()) {
        cleanup();
        return 17;
    }

    // ensure_default_camera_and_light on deserialized flat INST scene + idempotency.
    scene::Scene flat_scene;
    if (!scene::deserialize_scene(flat_scene_bytes, flat_scene).ok() ||
        flat_scene.active_camera().valid() ||
        !scene::ensure_default_camera_and_light(flat_scene).ok() ||
        !flat_scene.active_camera().valid() ||
        std::none_of(flat_scene.entities().begin(),
                     flat_scene.entities().end(),
                     [&](const scene::Entity candidate) noexcept {
                         return flat_scene.directional_light(candidate) != nullptr;
                     }) ||
        !flat_scene.validate().ok()) {
        cleanup();
        return 18;
    }
    const core::usize expected_entities = flat_scene.entity_count();
    if (!scene::ensure_default_camera_and_light(flat_scene).ok() ||
        flat_scene.entity_count() != expected_entities) {
        cleanup();
        return 19;
    }

    // Scene with an existing inactive camera + directional light activates existing camera.
    scene::Scene inactive_cam_scene;
    const scene::Entity cam_entity = inactive_cam_scene.create_entity();
    const scene::Entity light_entity = inactive_cam_scene.create_entity();
    if (!inactive_cam_scene.add_transform(cam_entity).ok() ||
        !inactive_cam_scene.add_camera(cam_entity, {.active = false}).ok() ||
        !inactive_cam_scene.add_transform(light_entity).ok() ||
        !inactive_cam_scene.add_directional_light(light_entity).ok() ||
        !scene::ensure_default_camera_and_light(inactive_cam_scene).ok() ||
        inactive_cam_scene.entity_count() != 2U ||
        inactive_cam_scene.active_camera() != cam_entity) {
        cleanup();
        return 20;
    }

    // Missing mesh/material reference fails validate_scene_dependencies.
    const auto missing_ref_scene_bytes = make_legacy_scene_bytes(0xdeadbeefULL, mat_id);
    SceneView missing_ref_scene_view{};
    if (!read_scene(missing_ref_scene_bytes, missing_ref_scene_view).ok() ||
        bundle.validate_scene_dependencies(missing_ref_scene_view, builtin_meshes, builtin_materials)
                .code != core::ErrorCode::invalid_argument) {
        cleanup();
        return 21;
    }

    // Missing normal_texture or orm_texture in material fails dependency validation.
    for (const std::array<u64, 3> textures : {
             std::array<u64, 3>{0x1111ULL, 0U, 0U},
             std::array<u64, 3>{tex_id, 0x2222ULL, 0U},
             std::array<u64, 3>{tex_id, 0U, 0x3333ULL},
         }) {
        PreparedAssetBundle incomplete;
        const auto m_bytes = make_mesh_bytes(false);
        const auto mat_missing_tex = make_material_bytes(textures[0], textures[1], textures[2]);
        MeshView mv{};
        MaterialView matv{};
        if (!read_mesh(m_bytes, mv).ok() || !read_material(mat_missing_tex, matv).ok()) {
            cleanup();
            return 22;
        }
        const std::filesystem::path tmp_dir = root / "incomplete";
        std::filesystem::remove_all(tmp_dir, error);
        if (!write_bytes(tmp_dir / "t.getex", texture_bytes) ||
            !write_bytes(tmp_dir / "m.gemesh", m_bytes) ||
            !write_bytes(tmp_dir / "mat.gemat", mat_missing_tex) ||
            !incomplete.load_directory(tmp_dir).ok()) {
            cleanup();
            return 23;
        }
        const auto ref_bytes =
            make_legacy_scene_bytes(mv.asset.asset_id, matv.asset.asset_id);
        SceneView ref_view{};
        if (!read_scene(ref_bytes, ref_view).ok() ||
            incomplete.validate_scene_dependencies(ref_view).code !=
                core::ErrorCode::invalid_argument) {
            cleanup();
            return 24;
        }
    }

    // Bridge view helper & clear.
    const auto render_assets = renderer::scene_bridge::make_prepared_render_assets(bundle);
    if (render_assets.meshes.size() != 2U || render_assets.textures.size() != 1U ||
        render_assets.materials.size() != 1U) {
        cleanup();
        return 25;
    }
    bundle.clear();
    if (bundle.total_count() != 0U || bundle.find_mesh(mesh_u16_view.asset.asset_id) != nullptr) {
        cleanup();
        return 26;
    }

    // Corrupted .getex in load_directory fails cleanly.
    const std::filesystem::path bad_file = root / "corrupt.getex";
    const std::array<std::byte, 4> junk = {
        std::byte{'B'}, std::byte{'A'}, std::byte{'D'}, std::byte{'!'}};
    if (!write_bytes(bad_file, junk) ||
        bundle.load_file(bad_file).code != core::ErrorCode::invalid_argument ||
        bundle.load_directory(root).code != core::ErrorCode::invalid_argument) {
        cleanup();
        return 27;
    }

    cleanup();
    return 0;
}
