#include "engine/assets/prepared_assets.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>

namespace gameengine::assets {

namespace {

[[nodiscard]] core::Status invalid() noexcept
{
    return core::Status{core::ErrorCode::invalid_argument};
}

[[nodiscard]] std::string lowercase_extension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    for (char& character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return extension;
}

[[nodiscard]] bool contains_id(std::span<const core::u64> ids, core::u64 target) noexcept
{
    for (const core::u64 id : ids) {
        if (id == target) {
            return true;
        }
    }
    return false;
}

template <typename AssetT>
[[nodiscard]] const AssetT* find_by_id(std::span<const AssetT> items, core::u64 asset_id) noexcept
{
    if (asset_id == 0U) {
        return nullptr;
    }
    for (const AssetT& item : items) {
        if (item.asset_id == asset_id) {
            return &item;
        }
    }
    return nullptr;
}

void append_submeshes(const MeshView& mesh, std::vector<MeshSubmeshView>& out_submeshes)
{
    out_submeshes.reserve(mesh.submesh_count);
    for (core::u32 index = 0; index < mesh.submesh_count; ++index) {
        const core::usize offset = static_cast<core::usize>(index) * 16U;
        out_submeshes.push_back(MeshSubmeshView{
            .first_index = read_u32(mesh.submeshes, offset),
            .index_count = read_u32(mesh.submeshes, offset + 4U),
            .material_slot = read_u32(mesh.submeshes, offset + 8U),
            .vertex_offset = read_u32(mesh.submeshes, offset + 12U),
        });
    }
}

} // namespace

core::Status read_binary_asset_file(const std::filesystem::path& path,
                                    std::vector<std::byte>& bytes,
                                    core::usize max_bytes) noexcept
{
    bytes.clear();
    if (path.empty() || max_bytes == 0U) {
        return invalid();
    }
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return invalid();
    }
    const auto file_size = std::filesystem::file_size(path, error);
    if (error || file_size == 0U || file_size > max_bytes) {
        return invalid();
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return invalid();
    }
    bytes.resize(static_cast<core::usize>(file_size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        bytes.clear();
        return invalid();
    }
    return core::Status{};
}

void PreparedAssetBundle::clear() noexcept
{
    meshes_.clear();
    textures_.clear();
    materials_.clear();
}

core::Status PreparedAssetBundle::load_file(const std::filesystem::path& path) noexcept
{
    const std::string extension = lowercase_extension(path);
    if (extension != ".gemesh" && extension != ".getex" && extension != ".gemat") {
        return invalid();
    }

    std::vector<std::byte> bytes;
    const core::Status read_status = read_binary_asset_file(path, bytes);
    if (!read_status) {
        return read_status;
    }

    if (extension == ".gemesh") {
        MeshView view{};
        if (!read_mesh(bytes, view) || view.asset.asset_id == 0U) {
            return invalid();
        }
        if (const PreparedMesh* existing = find_mesh(view.asset.asset_id); existing != nullptr) {
            return existing->content_hash == view.asset.content_hash ? core::Status{} : invalid();
        }
        if (total_count() >= max_prepared_asset_count) {
            return invalid();
        }
        PreparedMesh prepared{};
        prepared.asset_id = view.asset.asset_id;
        prepared.content_hash = view.asset.content_hash;
        prepared.vertex_count = view.vertex_count;
        prepared.vertex_stride = view.vertex_stride;
        prepared.index_format = view.index_format;
        prepared.index_count = view.index_count;
        prepared.vertices.assign(view.vertices.begin(), view.vertices.end());
        prepared.indices.assign(view.indices.begin(), view.indices.end());
        append_submeshes(view, prepared.submeshes);
        meshes_.push_back(std::move(prepared));
        return core::Status{};
    }

    if (extension == ".getex") {
        TextureView view{};
        if (!read_texture(bytes, view) || view.asset.asset_id == 0U) {
            return invalid();
        }
        if (const PreparedTexture* existing = find_texture(view.asset.asset_id);
            existing != nullptr) {
            return existing->content_hash == view.asset.content_hash ? core::Status{} : invalid();
        }
        if (total_count() >= max_prepared_asset_count) {
            return invalid();
        }
        PreparedTexture prepared{};
        prepared.asset_id = view.asset.asset_id;
        prepared.content_hash = view.asset.content_hash;
        prepared.width = view.width;
        prepared.height = view.height;
        prepared.mip_count = view.mip_count;
        prepared.format = view.format;
        prepared.pixels.assign(view.pixels.begin(), view.pixels.end());
        textures_.push_back(std::move(prepared));
        return core::Status{};
    }

    MaterialView view{};
    if (!read_material(bytes, view) || view.asset.asset_id == 0U) {
        return invalid();
    }
    if (const PreparedMaterial* existing = find_material(view.asset.asset_id);
        existing != nullptr) {
        return existing->content_hash == view.asset.content_hash ? core::Status{} : invalid();
    }
    if (total_count() >= max_prepared_asset_count) {
        return invalid();
    }
    PreparedMaterial prepared{};
    prepared.asset_id = view.asset.asset_id;
    prepared.content_hash = view.asset.content_hash;
    prepared.base_color_factor = view.base_color_factor;
    prepared.metallic = view.metallic;
    prepared.roughness = view.roughness;
    prepared.albedo_texture = view.albedo_texture;
    prepared.normal_texture = view.normal_texture;
    prepared.orm_texture = view.orm_texture;
    materials_.push_back(prepared);
    return core::Status{};
}

core::Status PreparedAssetBundle::load_directory(const std::filesystem::path& directory) noexcept
{
    if (directory.empty()) {
        return invalid();
    }
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
        return invalid();
    }

    std::vector<std::filesystem::path> asset_paths;
    std::filesystem::recursive_directory_iterator iterator(
        directory, std::filesystem::directory_options::skip_permission_denied, error);
    if (error) {
        return invalid();
    }
    for (const auto& entry : iterator) {
        if (!entry.is_regular_file(error) || error) {
            error.clear();
            continue;
        }
        const std::string extension = lowercase_extension(entry.path());
        if (extension == ".gemesh" || extension == ".getex" || extension == ".gemat") {
            asset_paths.push_back(entry.path());
            if (asset_paths.size() > max_prepared_asset_count * 2U) {
                return invalid();
            }
        }
    }

    std::sort(asset_paths.begin(), asset_paths.end());
    for (const std::filesystem::path& path : asset_paths) {
        const core::Status status = load_file(path);
        if (!status) {
            return status;
        }
    }
    return core::Status{};
}

core::Status PreparedAssetBundle::validate_scene_dependencies(
    const SceneView& scene,
    std::span<const core::u64> builtin_mesh_ids,
    std::span<const core::u64> builtin_material_ids) const noexcept
{
    std::vector<core::u64> available_meshes;
    available_meshes.reserve(builtin_mesh_ids.size() + meshes_.size());
    for (const core::u64 id : builtin_mesh_ids) {
        available_meshes.push_back(id);
    }
    for (const PreparedMesh& mesh : meshes_) {
        available_meshes.push_back(mesh.asset_id);
    }

    std::vector<core::u64> available_materials;
    available_materials.reserve(builtin_material_ids.size() + materials_.size());
    for (const core::u64 id : builtin_material_ids) {
        available_materials.push_back(id);
    }
    for (const PreparedMaterial& material : materials_) {
        available_materials.push_back(material.asset_id);
    }

    const core::Status ref_status = validate_scene_references(scene, available_meshes, available_materials);
    if (!ref_status) {
        return ref_status;
    }

    auto check_material_textures = [&](core::u64 material_id) noexcept {
        if (contains_id(builtin_material_ids, material_id)) {
            return true;
        }
        const PreparedMaterial* material = find_material(material_id);
        if (material == nullptr) {
            return false;
        }
        if (material->albedo_texture != 0U && find_texture(material->albedo_texture) == nullptr) {
            return false;
        }
        if (material->normal_texture != 0U && find_texture(material->normal_texture) == nullptr) {
            return false;
        }
        if (material->orm_texture != 0U && find_texture(material->orm_texture) == nullptr) {
            return false;
        }
        return true;
    };

    if (scene.extended) {
        for (core::usize offset = 0; offset < scene.mesh_renderers.size(); offset += 24U) {
            if (!check_material_textures(read_u64(scene.mesh_renderers, offset + 16U))) {
                return invalid();
            }
        }
    } else {
        for (core::u32 index = 0; index < scene.instance_count; ++index) {
            const core::usize offset = static_cast<core::usize>(index) * 80U;
            if (!check_material_textures(read_u64(scene.instances, offset + 72U))) {
                return invalid();
            }
        }
    }

    return core::Status{};
}

const PreparedMesh* PreparedAssetBundle::find_mesh(core::u64 asset_id) const noexcept
{
    return find_by_id<PreparedMesh>(meshes_, asset_id);
}

const PreparedTexture* PreparedAssetBundle::find_texture(core::u64 asset_id) const noexcept
{
    return find_by_id<PreparedTexture>(textures_, asset_id);
}

const PreparedMaterial* PreparedAssetBundle::find_material(core::u64 asset_id) const noexcept
{
    return find_by_id<PreparedMaterial>(materials_, asset_id);
}

core::usize PreparedAssetBundle::memory_usage_bytes() const noexcept
{
    core::usize total = meshes_.capacity() * sizeof(PreparedMesh) +
                        textures_.capacity() * sizeof(PreparedTexture) +
                        materials_.capacity() * sizeof(PreparedMaterial);
    for (const PreparedMesh& mesh : meshes_) {
        total += mesh.vertices.capacity() + mesh.indices.capacity() +
                 mesh.submeshes.capacity() * sizeof(MeshSubmeshView);
    }
    for (const PreparedTexture& texture : textures_) {
        total += texture.pixels.capacity();
    }
    return total;
}

} // namespace gameengine::assets
