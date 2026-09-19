#include "engine/assets/asset_reader.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

using gameengine::core::u32;
using gameengine::core::u64;

struct Chunk final {
    u32 kind = 0;
    std::vector<std::byte> data;
};

constexpr std::size_t align16(std::size_t value) noexcept
{
    return (value + 15U) & ~std::size_t{15U};
}

u64 hash_bytes(std::span<const std::byte> bytes, u64 hash = 0xcbf29ce484222325ULL) noexcept
{
    for (const std::byte byte : bytes) {
        hash ^= static_cast<u64>(std::to_integer<std::uint8_t>(byte));
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

void put_u32(std::vector<std::byte>& bytes, std::size_t offset, u32 value)
{
    bytes[offset] = std::byte{static_cast<unsigned char>(value & 0xffU)};
    bytes[offset + 1] = std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)};
    bytes[offset + 2] = std::byte{static_cast<unsigned char>((value >> 16U) & 0xffU)};
    bytes[offset + 3] = std::byte{static_cast<unsigned char>((value >> 24U) & 0xffU)};
}

void put_u64(std::vector<std::byte>& bytes, std::size_t offset, u64 value)
{
    for (std::size_t index = 0; index < sizeof(u64); ++index) {
        bytes[offset + index] = std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void append_u32(std::vector<std::byte>& bytes, u32 value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4);
    put_u32(bytes, offset, value);
}

void append_u64(std::vector<std::byte>& bytes, u64 value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 8);
    put_u64(bytes, offset, value);
}

void append_float(std::vector<std::byte>& bytes, float value)
{
    append_u32(bytes, std::bit_cast<u32>(value));
}

std::vector<std::byte> build_asset(std::array<std::byte, 4> magic, const std::vector<Chunk>& chunks)
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
    const u64 asset_id = hash_bytes(payload, hash_bytes(magic));
    std::vector<std::byte> output(cursor);
    for (std::size_t index = 0; index < magic.size(); ++index) {
        output[index] = magic[index];
    }
    put_u32(output, 4, 1U | (64U << 16U));
    put_u32(output, 8, 1U);
    put_u64(output, 12, output.size());
    put_u64(output, 20, asset_id);
    put_u64(output, 28, content_hash);
    put_u32(output, 36, static_cast<u32>(chunks.size()));
    put_u32(output, 40, 40U);
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        const std::size_t entry = 64U + index * 40U;
        const std::size_t offset = offsets[index];
        put_u32(output, entry, chunks[index].kind);
        put_u64(output, entry + 8, offset);
        put_u64(output, entry + 16, chunks[index].data.size());
        put_u64(output, entry + 24, chunks[index].data.size());
        put_u32(output, entry + 32, 16U);
        for (std::size_t byte = 0; byte < chunks[index].data.size(); ++byte) {
            output[offset + byte] = chunks[index].data[byte];
        }
    }
    return output;
}

constexpr u32 fourcc(char a, char b, char c, char d) noexcept
{
    return static_cast<u32>(a) | (static_cast<u32>(b) << 8U) |
        (static_cast<u32>(c) << 16U) | (static_cast<u32>(d) << 24U);
}

} // namespace

int main()
{
    using namespace gameengine::assets;

    const std::vector<std::byte> mesh_metadata = [] {
        std::vector<std::byte> data;
        for (const u32 value : {1U, 3U, 32U, 16U, 3U, 1U}) {
            append_u32(data, value);
        }
        return data;
    }();
    std::vector<std::byte> mesh_vertices;
    for (const float value : {
             -1.0F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
             1.0F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F,
             0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.5F, 0.0F}) {
        append_float(mesh_vertices, value);
    }
    std::vector<std::byte> mesh_indices;
    for (const u32 value : {0U, 1U, 2U}) {
        mesh_indices.push_back(std::byte{static_cast<unsigned char>(value)});
        mesh_indices.push_back(std::byte{0});
    }
    std::vector<std::byte> mesh_submeshes;
    for (const u32 value : {0U, 3U, 0U, 0U}) {
        append_u32(mesh_submeshes, value);
    }
    const auto mesh_bytes = build_asset(
        {std::byte{'G'}, std::byte{'M'}, std::byte{'S'}, std::byte{'H'}},
        {{fourcc('M', 'S', 'H', 'D'), mesh_metadata},
         {fourcc('V', 'E', 'R', 'T'), mesh_vertices},
         {fourcc('I', 'N', 'D', 'X'), mesh_indices},
         {fourcc('S', 'U', 'B', 'M'), mesh_submeshes}});
    MeshView mesh;
    if (!read_mesh(mesh_bytes, mesh).ok() || mesh.vertex_count != 3 || mesh.index_count != 3 ||
        mesh.vertices.size() != 96 || mesh.indices.size() != 6) {
        return 1;
    }

    const auto texture_bytes = build_asset(
        {std::byte{'G'}, std::byte{'T'}, std::byte{'E'}, std::byte{'X'}},
        {{fourcc('T', 'X', 'H', 'D'),
          {std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2}, std::byte{0},
           std::byte{0}, std::byte{0}, std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
           std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{16}, std::byte{0},
           std::byte{0}, std::byte{0}}},
         {fourcc('T', 'X', 'D', 'T'), std::vector<std::byte>(16, std::byte{255})}});
    TextureView texture;
    if (!read_texture(texture_bytes, texture).ok() || texture.width != 2 || texture.pixels.size() != 16) {
        return 2;
    }

    std::vector<std::byte> material_data;
    for (const float value : {1.0F, 1.0F, 1.0F, 1.0F, 0.1F, 0.5F}) {
        append_float(material_data, value);
    }
    material_data.resize(64);
    const auto material_bytes = build_asset(
        {std::byte{'G'}, std::byte{'M'}, std::byte{'A'}, std::byte{'T'}},
        {{fourcc('M', 'T', 'H', 'D'), material_data}});
    MaterialView material;
    if (!read_material(material_bytes, material).ok() || material.roughness != 0.5F) {
        return 3;
    }

    std::vector<std::byte> scene_header;
    append_u32(scene_header, 1);
    append_u32(scene_header, 0);
    std::vector<std::byte> scene_instances(80);
    put_u64(scene_instances, 64, mesh.asset.asset_id);
    put_u64(scene_instances, 72, material.asset.asset_id);
    const auto scene_bytes = build_asset(
        {std::byte{'G'}, std::byte{'S'}, std::byte{'C'}, std::byte{'N'}},
        {{fourcc('S', 'C', 'H', 'D'), scene_header},
         {fourcc('I', 'N', 'S', 'T'), scene_instances}});
    SceneView scene;
    if (!read_scene(scene_bytes, scene).ok()) {
        return 4;
    }
    const std::array mesh_ids{mesh.asset.asset_id};
    const std::array material_ids{material.asset.asset_id};
    if (!validate_scene_references(scene, mesh_ids, material_ids).ok()) {
        return 5;
    }

    auto truncated = mesh_bytes;
    truncated.pop_back();
    AssetView invalid_view;
    if (validate_container(truncated, AssetKind::mesh, invalid_view).ok()) {
        return 6;
    }
    auto invalid_flags = mesh_bytes;
    invalid_flags[8] = std::byte{2};
    if (validate_container(invalid_flags, AssetKind::mesh, invalid_view).ok()) {
        return 7;
    }
    auto invalid_version = mesh_bytes;
    invalid_version[4] = std::byte{2};
    if (validate_container(invalid_version, AssetKind::mesh, invalid_view).ok()) {
        return 8;
    }
    auto overlapping_chunks = mesh_bytes;
    put_u64(overlapping_chunks, 64U + 40U + 8U, read_u64(mesh_bytes, 64U + 8U));
    if (validate_container(overlapping_chunks, AssetKind::mesh, invalid_view).ok()) {
        return 9;
    }
    auto wrong_kind = mesh_bytes;
    wrong_kind[0] = std::byte{'G'};
    wrong_kind[1] = std::byte{'T'};
    if (validate_container(wrong_kind, AssetKind::mesh, invalid_view).ok()) {
        return 10;
    }

    return 0;
}
