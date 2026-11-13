#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

#include "engine/assets/asset_reader.hpp"
#include "engine/core/status.hpp"

namespace gameengine::assets {

inline constexpr core::usize max_prepared_asset_file_bytes = 64U * 1024U * 1024U;
inline constexpr core::usize max_prepared_asset_count = 4096U;

struct PreparedMesh final {
    core::u64 asset_id = 0;
    core::u64 content_hash = 0;
    core::u32 vertex_count = 0;
    core::u32 vertex_stride = 0;
    MeshIndexFormat index_format = MeshIndexFormat::u16;
    core::u32 index_count = 0;
    std::vector<std::byte> vertices{};
    std::vector<std::byte> indices{};
    std::vector<MeshSubmeshView> submeshes{};
};

struct PreparedTexture final {
    core::u64 asset_id = 0;
    core::u64 content_hash = 0;
    core::u32 width = 0;
    core::u32 height = 0;
    core::u32 mip_count = 0;
    core::u32 format = 0;
    std::vector<std::byte> pixels{};
};

struct PreparedMaterial final {
    core::u64 asset_id = 0;
    core::u64 content_hash = 0;
    std::array<float, 4> base_color_factor = {1.0F, 1.0F, 1.0F, 1.0F};
    float metallic = 0.05F;
    float roughness = 0.42F;
    core::u64 albedo_texture = 0;
    core::u64 normal_texture = 0;
    core::u64 orm_texture = 0;
};

[[nodiscard]] core::Status read_binary_asset_file(
    const std::filesystem::path& path,
    std::vector<std::byte>& bytes,
    core::usize max_bytes = max_prepared_asset_file_bytes) noexcept;

class PreparedAssetBundle final {
public:
    void clear() noexcept;

    [[nodiscard]] core::Status load_file(const std::filesystem::path& path) noexcept;
    [[nodiscard]] core::Status load_directory(const std::filesystem::path& directory) noexcept;

    [[nodiscard]] core::Status validate_scene_dependencies(
        const SceneView& scene,
        std::span<const core::u64> builtin_mesh_ids = {},
        std::span<const core::u64> builtin_material_ids = {}) const noexcept;

    [[nodiscard]] const PreparedMesh* find_mesh(core::u64 asset_id) const noexcept;
    [[nodiscard]] const PreparedTexture* find_texture(core::u64 asset_id) const noexcept;
    [[nodiscard]] const PreparedMaterial* find_material(core::u64 asset_id) const noexcept;

    [[nodiscard]] std::span<const PreparedMesh> meshes() const noexcept { return meshes_; }
    [[nodiscard]] std::span<const PreparedTexture> textures() const noexcept { return textures_; }
    [[nodiscard]] std::span<const PreparedMaterial> materials() const noexcept
    {
        return materials_;
    }

    [[nodiscard]] core::usize total_count() const noexcept
    {
        return meshes_.size() + textures_.size() + materials_.size();
    }

    [[nodiscard]] core::usize memory_usage_bytes() const noexcept;

private:
    std::vector<PreparedMesh> meshes_{};
    std::vector<PreparedTexture> textures_{};
    std::vector<PreparedMaterial> materials_{};
};

} // namespace gameengine::assets
