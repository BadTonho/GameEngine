#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "engine/core/status.hpp"

namespace gameengine::assets {

enum class AssetKind : core::u32 {
    mesh,
    texture,
    material,
    scene,
    package,
};

enum class MeshIndexFormat : core::u32 {
    u16 = 16,
    u32 = 32,
};

constexpr core::u32 asset_fourcc(char a, char b, char c, char d) noexcept
{
    return static_cast<core::u32>(a) | (static_cast<core::u32>(b) << 8U) |
        (static_cast<core::u32>(c) << 16U) | (static_cast<core::u32>(d) << 24U);
}

inline constexpr core::u32 scene_chunk_header = asset_fourcc('S', 'C', 'H', 'D');
inline constexpr core::u32 scene_chunk_instances = asset_fourcc('I', 'N', 'S', 'T');
inline constexpr core::u32 scene_chunk_entities = asset_fourcc('E', 'N', 'T', 'S');
inline constexpr core::u32 scene_chunk_transforms = asset_fourcc('T', 'R', 'N', 'S');
inline constexpr core::u32 scene_chunk_mesh_renderers = asset_fourcc('M', 'E', 'S', 'H');
inline constexpr core::u32 scene_chunk_cameras = asset_fourcc('C', 'A', 'M', 'R');
inline constexpr core::u32 scene_chunk_lights = asset_fourcc('L', 'I', 'T', 'E');

struct ChunkView final {
    core::u32 kind = 0;
    core::u32 flags = 0;
    std::span<const std::byte> data{};
};

struct AssetView final {
    AssetKind kind = AssetKind::mesh;
    core::u16 version = 0;
    core::u64 asset_id = 0;
    core::u64 content_hash = 0;
    std::span<const std::byte> bytes{};
    std::array<ChunkView, 64> chunks{};
    core::u32 chunk_count = 0;
};

struct MeshSubmeshView final {
    core::u32 first_index = 0;
    core::u32 index_count = 0;
    core::u32 material_slot = 0;
    core::u32 vertex_offset = 0;
};

struct MeshView final {
    AssetView asset{};
    core::u32 vertex_count = 0;
    core::u32 vertex_stride = 0;
    MeshIndexFormat index_format = MeshIndexFormat::u16;
    core::u32 index_count = 0;
    core::u32 submesh_count = 0;
    std::span<const std::byte> vertices{};
    std::span<const std::byte> indices{};
    std::span<const std::byte> submeshes{};
};

struct TextureView final {
    AssetView asset{};
    core::u32 width = 0;
    core::u32 height = 0;
    core::u32 mip_count = 0;
    core::u32 format = 0;
    std::span<const std::byte> pixels{};
};

struct MaterialView final {
    AssetView asset{};
    std::array<float, 4> base_color_factor{};
    float metallic = 0.0F;
    float roughness = 0.0F;
    core::u64 albedo_texture = 0;
    core::u64 normal_texture = 0;
    core::u64 orm_texture = 0;
};

struct SceneView final {
    AssetView asset{};
    core::u32 instance_count = 0;
    std::span<const std::byte> instances{};
    bool extended = false;
    core::u64 active_camera_value = 0;
    core::u32 entity_count = 0;
    std::span<const std::byte> entities{};
    std::span<const std::byte> transforms{};
    std::span<const std::byte> mesh_renderers{};
    std::span<const std::byte> cameras{};
    std::span<const std::byte> lights{};
};

[[nodiscard]] core::Status validate_container(
    std::span<const std::byte> bytes,
    AssetKind expected_kind,
    AssetView& view) noexcept;

[[nodiscard]] const ChunkView* find_chunk(const AssetView& asset, core::u32 kind) noexcept;

[[nodiscard]] core::Status read_mesh(
    std::span<const std::byte> bytes,
    MeshView& view) noexcept;

[[nodiscard]] core::Status read_texture(
    std::span<const std::byte> bytes,
    TextureView& view) noexcept;

[[nodiscard]] core::Status read_material(
    std::span<const std::byte> bytes,
    MaterialView& view) noexcept;

[[nodiscard]] core::Status read_scene(
    std::span<const std::byte> bytes,
    SceneView& view) noexcept;

[[nodiscard]] core::Status validate_scene_references(
    const SceneView& scene,
    std::span<const core::u64> mesh_ids,
    std::span<const core::u64> material_ids) noexcept;

[[nodiscard]] core::u32 read_u32(std::span<const std::byte> bytes, core::usize offset) noexcept;
[[nodiscard]] core::u64 read_u64(std::span<const std::byte> bytes, core::usize offset) noexcept;

} // namespace gameengine::assets
