#include "engine/assets/asset_reader.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace gameengine::assets {

namespace {

constexpr core::u16 asset_version = 1;
constexpr core::u16 header_size = 64;
constexpr core::u32 chunk_entry_size = 40;
constexpr core::u32 max_chunks = 64;
constexpr core::u32 required_flags = 1;
constexpr core::usize alignment = 16;
constexpr core::usize max_file_size = 1ULL << 30U;

constexpr core::u32 mesh_header_chunk =
    static_cast<core::u32>('M') | (static_cast<core::u32>('S') << 8U) |
    (static_cast<core::u32>('H') << 16U) | (static_cast<core::u32>('D') << 24U);
constexpr core::u32 vertices_chunk =
    static_cast<core::u32>('V') | (static_cast<core::u32>('E') << 8U) |
    (static_cast<core::u32>('R') << 16U) | (static_cast<core::u32>('T') << 24U);
constexpr core::u32 indices_chunk =
    static_cast<core::u32>('I') | (static_cast<core::u32>('N') << 8U) |
    (static_cast<core::u32>('D') << 16U) | (static_cast<core::u32>('X') << 24U);
constexpr core::u32 submeshes_chunk =
    static_cast<core::u32>('S') | (static_cast<core::u32>('U') << 8U) |
    (static_cast<core::u32>('B') << 16U) | (static_cast<core::u32>('M') << 24U);
constexpr core::u32 texture_header_chunk =
    static_cast<core::u32>('T') | (static_cast<core::u32>('X') << 8U) |
    (static_cast<core::u32>('H') << 16U) | (static_cast<core::u32>('D') << 24U);
constexpr core::u32 texture_data_chunk =
    static_cast<core::u32>('T') | (static_cast<core::u32>('X') << 8U) |
    (static_cast<core::u32>('D') << 16U) | (static_cast<core::u32>('T') << 24U);
constexpr core::u32 material_header_chunk =
    static_cast<core::u32>('M') | (static_cast<core::u32>('T') << 8U) |
    (static_cast<core::u32>('H') << 16U) | (static_cast<core::u32>('D') << 24U);
constexpr core::u32 scene_header_chunk =
    static_cast<core::u32>('S') | (static_cast<core::u32>('C') << 8U) |
    (static_cast<core::u32>('H') << 16U) | (static_cast<core::u32>('D') << 24U);
constexpr core::u32 instances_chunk =
    static_cast<core::u32>('I') | (static_cast<core::u32>('N') << 8U) |
    (static_cast<core::u32>('S') << 16U) | (static_cast<core::u32>('T') << 24U);

constexpr std::array<std::array<std::byte, 4>, 5> magic_values = {{
    {{std::byte{'G'}, std::byte{'M'}, std::byte{'S'}, std::byte{'H'}}},
    {{std::byte{'G'}, std::byte{'T'}, std::byte{'E'}, std::byte{'X'}}},
    {{std::byte{'G'}, std::byte{'M'}, std::byte{'A'}, std::byte{'T'}}},
    {{std::byte{'G'}, std::byte{'S'}, std::byte{'C'}, std::byte{'N'}}},
    {{std::byte{'G'}, std::byte{'P'}, std::byte{'A'}, std::byte{'K'}}},
}};

[[nodiscard]] core::Status invalid() noexcept
{
    return core::Status{core::ErrorCode::invalid_argument};
}

[[nodiscard]] bool checked_range(
    core::usize offset,
    core::usize size,
    core::usize limit,
    core::usize& end) noexcept
{
    if (offset > limit || size > limit - offset) {
        return false;
    }
    end = offset + size;
    return true;
}

[[nodiscard]] bool align_up(core::usize value, core::usize& aligned) noexcept
{
    if (value > std::numeric_limits<core::usize>::max() - (alignment - 1U)) {
        return false;
    }
    aligned = (value + alignment - 1U) & ~(alignment - 1U);
    return true;
}

[[nodiscard]] core::u64 fnv1a64(std::span<const std::byte> bytes, core::u64 hash) noexcept
{
    for (const std::byte byte : bytes) {
        hash ^= static_cast<core::u64>(std::to_integer<core::u8>(byte));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

[[nodiscard]] bool magic_to_kind(std::span<const std::byte> magic, AssetKind& kind) noexcept
{
    for (core::usize index = 0; index < magic_values.size(); ++index) {
        if (std::equal(magic.begin(), magic.end(), magic_values[index].begin(), magic_values[index].end())) {
            kind = static_cast<AssetKind>(index);
            return true;
        }
    }
    return false;
}

[[nodiscard]] const ChunkView* find_chunk_pair(
    const AssetView& asset,
    core::u32 kind,
    core::u32 occurrence = 0) noexcept
{
    for (core::u32 index = 0; index < asset.chunk_count; ++index) {
        if (asset.chunks[index].kind == kind) {
            if (occurrence == 0) {
                return &asset.chunks[index];
            }
            --occurrence;
        }
    }
    return nullptr;
}

[[nodiscard]] float read_f32(std::span<const std::byte> bytes, core::usize offset) noexcept
{
    const core::u32 bits = read_u32(bytes, offset);
    return std::bit_cast<float>(bits);
}

[[nodiscard]] bool contains_id(std::span<const core::u64> ids, core::u64 value) noexcept
{
    return std::find(ids.begin(), ids.end(), value) != ids.end();
}

} // namespace

core::u32 read_u32(std::span<const std::byte> bytes, core::usize offset) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < sizeof(core::u32)) {
        return 0;
    }
    return static_cast<core::u32>(std::to_integer<core::u8>(bytes[offset])) |
        (static_cast<core::u32>(std::to_integer<core::u8>(bytes[offset + 1])) << 8U) |
        (static_cast<core::u32>(std::to_integer<core::u8>(bytes[offset + 2])) << 16U) |
        (static_cast<core::u32>(std::to_integer<core::u8>(bytes[offset + 3])) << 24U);
}

core::u64 read_u64(std::span<const std::byte> bytes, core::usize offset) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < sizeof(core::u64)) {
        return 0;
    }
    core::u64 value = 0;
    for (core::usize index = 0; index < sizeof(core::u64); ++index) {
        value |= static_cast<core::u64>(std::to_integer<core::u8>(bytes[offset + index])) << (index * 8U);
    }
    return value;
}

core::Status validate_container(
    std::span<const std::byte> bytes,
    AssetKind expected_kind,
    AssetView& view) noexcept
{
    view = AssetView{};
    if (bytes.size() < header_size || bytes.size() > max_file_size) {
        return invalid();
    }
    AssetKind actual_kind = AssetKind::mesh;
    if (!magic_to_kind(bytes.first(4), actual_kind) || actual_kind != expected_kind) {
        return invalid();
    }
    const core::u32 version_and_header = read_u32(bytes, 4);
    if ((version_and_header & 0xffffU) != asset_version ||
        (version_and_header >> 16U) != header_size) {
        return invalid();
    }
    for (core::usize offset = 44; offset < header_size; ++offset) {
        if (bytes[offset] != std::byte{0}) {
            return invalid();
        }
    }
    if (read_u32(bytes, 8) != required_flags || read_u64(bytes, 12) != bytes.size()) {
        return invalid();
    }
    const core::u64 asset_id = read_u64(bytes, 20);
    const core::u64 content_hash = read_u64(bytes, 28);
    const core::u32 chunk_count = read_u32(bytes, 36);
    if (chunk_count == 0 || chunk_count > max_chunks || read_u32(bytes, 40) != chunk_entry_size) {
        return invalid();
    }
    core::usize table_size = 0;
    if (chunk_count > std::numeric_limits<core::usize>::max() / chunk_entry_size ||
        !checked_range(header_size, static_cast<core::usize>(chunk_count) * chunk_entry_size, bytes.size(), table_size)) {
        return invalid();
    }
    core::usize payload_begin = 0;
    if (!align_up(table_size, payload_begin) || payload_begin > bytes.size()) {
        return invalid();
    }

    struct Range final {
        core::usize start = 0;
        core::usize end = 0;
    };
    std::array<Range, max_chunks> ranges{};
    core::u64 payload_hash = 0xcbf29ce484222325ULL;
    core::u32 parsed_count = 0;
    for (core::u32 index = 0; index < chunk_count; ++index) {
        const core::usize entry = header_size + static_cast<core::usize>(index) * chunk_entry_size;
        const core::u32 flags = read_u32(bytes, entry + 4);
        const core::u64 offset_value = read_u64(bytes, entry + 8);
        const core::u64 size_value = read_u64(bytes, entry + 16);
        const core::u64 original_size_value = read_u64(bytes, entry + 24);
        if (offset_value > std::numeric_limits<core::usize>::max() ||
            size_value > std::numeric_limits<core::usize>::max() ||
            original_size_value > std::numeric_limits<core::usize>::max()) {
            return invalid();
        }
        const core::usize offset = static_cast<core::usize>(offset_value);
        const core::usize size = static_cast<core::usize>(size_value);
        const core::usize original_size = static_cast<core::usize>(original_size_value);
        if (flags != 0 || read_u32(bytes, entry + 36) != 0 || original_size != size ||
            read_u32(bytes, entry + 32) != alignment ||
            offset % alignment != 0) {
            return invalid();
        }
        core::usize end = 0;
        if (offset < payload_begin || !checked_range(offset, size, bytes.size(), end)) {
            return invalid();
        }
        for (core::u32 previous = 0; previous < parsed_count; ++previous) {
            if (offset < ranges[previous].end && ranges[previous].start < end) {
                return invalid();
            }
        }
        ranges[parsed_count++] = Range{offset, end};
        const auto chunk_bytes = bytes.subspan(offset, size);
        payload_hash = fnv1a64(chunk_bytes, payload_hash);
        view.chunks[index] = ChunkView{read_u32(bytes, entry), flags, chunk_bytes};
    }
    if (payload_hash != content_hash) {
        return invalid();
    }
    core::u64 identity_hash = fnv1a64(bytes.first(4), 0xcbf29ce484222325ULL);
    for (core::u32 index = 0; index < chunk_count; ++index) {
        identity_hash = fnv1a64(view.chunks[index].data, identity_hash);
    }
    if (identity_hash != asset_id) {
        return invalid();
    }
    view.kind = actual_kind;
    view.version = asset_version;
    view.asset_id = asset_id;
    view.content_hash = content_hash;
    view.bytes = bytes;
    view.chunk_count = chunk_count;
    return {};
}

const ChunkView* find_chunk(const AssetView& asset, core::u32 kind) noexcept
{
    return find_chunk_pair(asset, kind);
}

core::Status read_mesh(std::span<const std::byte> bytes, MeshView& view) noexcept
{
    view = MeshView{};
    if (validate_container(bytes, AssetKind::mesh, view.asset).code != core::ErrorCode::none) {
        return invalid();
    }
    const ChunkView* metadata = find_chunk_pair(view.asset, mesh_header_chunk);
    const ChunkView* vertices = find_chunk_pair(view.asset, vertices_chunk);
    const ChunkView* indices = find_chunk_pair(view.asset, indices_chunk);
    const ChunkView* submeshes = find_chunk_pair(view.asset, submeshes_chunk);
    if (metadata == nullptr || vertices == nullptr || indices == nullptr || submeshes == nullptr ||
        metadata->data.size() != 24) {
        return invalid();
    }
    view.vertex_count = read_u32(metadata->data, 4);
    view.vertex_stride = read_u32(metadata->data, 8);
    view.index_count = read_u32(metadata->data, 16);
    view.submesh_count = read_u32(metadata->data, 20);
    if (read_u32(metadata->data, 0) != 1 || view.vertex_stride != 32 || view.vertex_count == 0) {
        return invalid();
    }
    const core::u32 index_format = read_u32(metadata->data, 12);
    if (index_format != 16 && index_format != 32) {
        return invalid();
    }
    view.index_format = static_cast<MeshIndexFormat>(index_format);
    const core::usize index_width = index_format == 16 ? 2 : 4;
    if (view.vertex_count > std::numeric_limits<core::usize>::max() / view.vertex_stride ||
        view.index_count > std::numeric_limits<core::usize>::max() / index_width ||
        view.submesh_count > std::numeric_limits<core::usize>::max() / 16 ||
        vertices->data.size() != static_cast<core::usize>(view.vertex_count) * view.vertex_stride ||
        indices->data.size() != static_cast<core::usize>(view.index_count) * index_width ||
        submeshes->data.size() != static_cast<core::usize>(view.submesh_count) * 16) {
        return invalid();
    }
    for (core::u32 index = 0; index < view.index_count; ++index) {
        const core::usize offset = static_cast<core::usize>(index) * index_width;
        const core::u32 value = index_format == 16
            ? static_cast<core::u32>(std::to_integer<core::u8>(indices->data[offset])) |
                (static_cast<core::u32>(std::to_integer<core::u8>(indices->data[offset + 1])) << 8U)
            : read_u32(indices->data, offset);
        if (value >= view.vertex_count) {
            return invalid();
        }
    }
    for (core::u32 index = 0; index < view.submesh_count; ++index) {
        const core::usize offset = static_cast<core::usize>(index) * 16;
        const core::u64 end = static_cast<core::u64>(read_u32(submeshes->data, offset)) + read_u32(submeshes->data, offset + 4);
        if (end > view.index_count || read_u32(submeshes->data, offset + 12) >= view.vertex_count) {
            return invalid();
        }
    }
    view.vertices = vertices->data;
    view.indices = indices->data;
    view.submeshes = submeshes->data;
    return {};
}

core::Status read_texture(std::span<const std::byte> bytes, TextureView& view) noexcept
{
    view = TextureView{};
    if (validate_container(bytes, AssetKind::texture, view.asset).code != core::ErrorCode::none) {
        return invalid();
    }
    const ChunkView* metadata = find_chunk_pair(view.asset, texture_header_chunk);
    const ChunkView* pixels = find_chunk_pair(view.asset, texture_data_chunk);
    if (metadata == nullptr || pixels == nullptr || metadata->data.size() != 20) {
        return invalid();
    }
    view.width = read_u32(metadata->data, 0);
    view.height = read_u32(metadata->data, 4);
    view.mip_count = read_u32(metadata->data, 8);
    view.format = read_u32(metadata->data, 12);
    if (view.width == 0 || view.height == 0 || view.width > 16384 || view.height > 16384 ||
        view.mip_count == 0 || view.format != 1) {
        return invalid();
    }
    const core::u64 expected = static_cast<core::u64>(view.width) * view.height * 4;
    if (expected != pixels->data.size()) {
        return invalid();
    }
    view.pixels = pixels->data;
    return {};
}

core::Status read_material(std::span<const std::byte> bytes, MaterialView& view) noexcept
{
    view = MaterialView{};
    if (validate_container(bytes, AssetKind::material, view.asset).code != core::ErrorCode::none) {
        return invalid();
    }
    const ChunkView* metadata = find_chunk_pair(view.asset, material_header_chunk);
    if (metadata == nullptr || metadata->data.size() != 64) {
        return invalid();
    }
    for (core::usize index = 0; index < view.base_color_factor.size(); ++index) {
        view.base_color_factor[index] = read_f32(metadata->data, index * 4);
    }
    view.metallic = read_f32(metadata->data, 16);
    view.roughness = read_f32(metadata->data, 20);
    view.albedo_texture = read_u64(metadata->data, 24);
    view.normal_texture = read_u64(metadata->data, 32);
    view.orm_texture = read_u64(metadata->data, 40);
    if (!std::all_of(view.base_color_factor.begin(), view.base_color_factor.end(), [](float value) { return std::isfinite(value); }) ||
        !std::isfinite(view.metallic) || !std::isfinite(view.roughness) || view.metallic < 0.0F || view.metallic > 1.0F ||
        view.roughness < 0.0F || view.roughness > 1.0F) {
        return invalid();
    }
    return {};
}

core::Status read_scene(std::span<const std::byte> bytes, SceneView& view) noexcept
{
    view = SceneView{};
    if (validate_container(bytes, AssetKind::scene, view.asset).code != core::ErrorCode::none) {
        return invalid();
    }
    const ChunkView* metadata = find_chunk_pair(view.asset, scene_header_chunk);
    const ChunkView* instances = find_chunk_pair(view.asset, instances_chunk);
    if (metadata == nullptr || instances == nullptr || metadata->data.size() != 8) {
        return invalid();
    }
    view.instance_count = read_u32(metadata->data, 0);
    if (view.instance_count > std::numeric_limits<core::usize>::max() / 80 ||
        instances->data.size() != static_cast<core::usize>(view.instance_count) * 80) {
        return invalid();
    }
    view.instances = instances->data;
    return {};
}

core::Status validate_scene_references(
    const SceneView& scene,
    std::span<const core::u64> mesh_ids,
    std::span<const core::u64> material_ids) noexcept
{
    for (core::u32 index = 0; index < scene.instance_count; ++index) {
        const core::usize offset = static_cast<core::usize>(index) * 80;
        const core::u64 mesh_id = read_u64(scene.instances, offset + 64);
        const core::u64 material_id = read_u64(scene.instances, offset + 72);
        if (!contains_id(mesh_ids, mesh_id) || !contains_id(material_ids, material_id)) {
            return core::Status{core::ErrorCode::invalid_argument};
        }
    }
    return {};
}

} // namespace gameengine::assets
