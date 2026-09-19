#include "engine/editor/asset_catalog.hpp"
#include "engine/editor/console.hpp"
#include "engine/editor/profiler.hpp"
#include "engine/editor/project.hpp"
#include "engine/editor/ui.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

using gameengine::core::u32;
using gameengine::core::u64;

constexpr u64 fnv_offset = 0xcbf29ce484222325ULL;
constexpr u64 fnv_prime = 0x100000001b3ULL;

u64 hash_bytes(std::span<const std::byte> bytes, u64 hash = fnv_offset) noexcept
{
    for (const std::byte byte : bytes) {
        hash ^= static_cast<u64>(std::to_integer<unsigned char>(byte));
        hash *= fnv_prime;
    }
    return hash;
}

void put_u32(std::vector<std::byte>& bytes, std::size_t offset, u32 value)
{
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes[offset + index] = std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void put_u64(std::vector<std::byte>& bytes, std::size_t offset, u64 value)
{
    for (std::size_t index = 0; index < 8U; ++index) {
        bytes[offset + index] = std::byte{static_cast<unsigned char>((value >> (index * 8U)) & 0xffU)};
    }
}

void append_u32(std::vector<std::byte>& bytes, u32 value)
{
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4U);
    put_u32(bytes, offset, value);
}

void append_float(std::vector<std::byte>& bytes, float value)
{
    append_u32(bytes, std::bit_cast<u32>(value));
}

constexpr u32 fourcc(char a, char b, char c, char d) noexcept
{
    return static_cast<u32>(a) | (static_cast<u32>(b) << 8U) |
        (static_cast<u32>(c) << 16U) | (static_cast<u32>(d) << 24U);
}

struct Chunk final {
    u32 kind = 0;
    std::vector<std::byte> bytes{};
};

std::vector<std::byte> build_asset(const std::array<std::byte, 4>& magic,
                                   const std::vector<Chunk>& chunks)
{
    const std::size_t table_end = 64U + chunks.size() * 40U;
    std::size_t cursor = (table_end + 15U) & ~std::size_t{15U};
    std::vector<std::size_t> offsets;
    offsets.reserve(chunks.size());
    for (const Chunk& chunk : chunks) {
        cursor = (cursor + 15U) & ~std::size_t{15U};
        offsets.push_back(cursor);
        cursor += chunk.bytes.size();
    }
    std::vector<std::byte> output(cursor);
    for (std::size_t index = 0; index < magic.size(); ++index) {
        output[index] = magic[index];
    }
    put_u32(output, 4, 1U | (64U << 16U));
    put_u32(output, 8, 1U);
    put_u64(output, 12, output.size());
    std::vector<std::byte> payload;
    for (const Chunk& chunk : chunks) {
        payload.insert(payload.end(), chunk.bytes.begin(), chunk.bytes.end());
    }
    put_u64(output, 20, hash_bytes(payload, hash_bytes(magic)));
    put_u64(output, 28, hash_bytes(payload));
    put_u32(output, 36, static_cast<u32>(chunks.size()));
    put_u32(output, 40, 40U);
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        const std::size_t entry = 64U + index * 40U;
        put_u32(output, entry, chunks[index].kind);
        put_u64(output, entry + 8U, offsets[index]);
        put_u64(output, entry + 16U, chunks[index].bytes.size());
        put_u64(output, entry + 24U, chunks[index].bytes.size());
        put_u32(output, entry + 32U, 16U);
        std::copy(chunks[index].bytes.begin(),
                  chunks[index].bytes.end(),
                  output.begin() + static_cast<std::ptrdiff_t>(offsets[index]));
    }
    return output;
}

void write_bytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::byte> valid_mesh()
{
    std::vector<std::byte> metadata;
    for (const u32 value : {1U, 3U, 32U, 16U, 3U, 1U}) {
        append_u32(metadata, value);
    }
    std::vector<std::byte> vertices;
    for (const float value : {
             -1.0F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
             1.0F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F,
             0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.5F, 0.0F}) {
        append_float(vertices, value);
    }
    std::vector<std::byte> indices(6U, std::byte{0});
    indices[2] = std::byte{1};
    indices[4] = std::byte{2};
    std::vector<std::byte> submeshes;
    for (const u32 value : {0U, 3U, 0U, 0U}) {
        append_u32(submeshes, value);
    }
    return build_asset({std::byte{'G'}, std::byte{'M'}, std::byte{'S'}, std::byte{'H'}},
                       {{fourcc('M', 'S', 'H', 'D'), metadata},
                        {fourcc('V', 'E', 'R', 'T'), vertices},
                        {fourcc('I', 'N', 'D', 'X'), indices},
                        {fourcc('S', 'U', 'B', 'M'), submeshes}});
}

std::vector<std::byte> valid_texture()
{
    std::vector<std::byte> metadata;
    for (const u32 value : {2U, 2U, 1U, 1U, 16U}) {
        append_u32(metadata, value);
    }
    return build_asset({std::byte{'G'}, std::byte{'T'}, std::byte{'E'}, std::byte{'X'}},
                       {{fourcc('T', 'X', 'H', 'D'), metadata},
                        {fourcc('T', 'X', 'D', 'T'), std::vector<std::byte>(16U, std::byte{255})}});
}

std::vector<std::byte> valid_material()
{
    std::vector<std::byte> metadata;
    for (const float value : {1.0F, 1.0F, 1.0F, 1.0F, 0.1F, 0.5F}) {
        append_float(metadata, value);
    }
    metadata.resize(64U);
    return build_asset({std::byte{'G'}, std::byte{'M'}, std::byte{'A'}, std::byte{'T'}},
                       {{fourcc('M', 'T', 'H', 'D'), metadata}});
}

bool expect(bool value) noexcept
{
    return value;
}

} // namespace

int main()
{
    using namespace gameengine;
    using namespace gameengine::editor;
    std::error_code error;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path(error) / "gameengine_editor_tools_test";
    if (error) {
        return 1;
    }
    std::filesystem::remove_all(directory, error);
    ProjectSession project;
    if (!expect(project.create_new(directory).ok())) {
        return 2;
    }
    const std::filesystem::path asset_directory = directory / "assets";
    std::filesystem::create_directories(asset_directory, error);
    if (error) {
        std::filesystem::remove_all(directory, error);
        return 3;
    }
    write_bytes(asset_directory / "cube.gemesh", valid_mesh());
    write_bytes(asset_directory / "checker.getex", valid_texture());
    write_bytes(asset_directory / "material.gemat", valid_material());
    const std::array<std::byte, 3> invalid_bytes{std::byte{'b'}, std::byte{'a'}, std::byte{'d'}};
    write_bytes(asset_directory / "broken.gemesh", invalid_bytes);

    AssetCatalog catalog;
    if (!expect(catalog.refresh(directory).ok()) ||
        !expect(catalog.records().size() == 5U) ||
        !expect(catalog.records()[0].relative_path < catalog.records()[1].relative_path)) {
        std::filesystem::remove_all(directory, error);
        return 4;
    }
    core::u32 invalid_count = 0;
    for (const AssetRecord& record : catalog.records()) {
        invalid_count += record.valid ? 0U : 1U;
    }
    if (!expect(invalid_count == 1U) || !expect(catalog.select(0U).ok()) ||
        !expect(catalog.selected() != nullptr)) {
        std::filesystem::remove_all(directory, error);
        return 5;
    }
    const std::string selected_path = catalog.selected()->relative_path;
    if (!expect(catalog.refresh(directory).ok()) || !expect(catalog.selected() != nullptr) ||
        !expect(catalog.selected()->relative_path == selected_path)) {
        std::filesystem::remove_all(directory, error);
        return 6;
    }

    ConsoleBuffer console;
    for (u32 index = 0; index < ConsoleBuffer::capacity + 2U; ++index) {
        console.push(core::LogLevel::info, std::to_string(index));
    }
    if (!expect(console.size() == ConsoleBuffer::capacity) ||
        !expect(console.message(0).text == "2") ||
        !expect(console.message(ConsoleBuffer::capacity - 1U).text == "65")) {
        std::filesystem::remove_all(directory, error);
        return 7;
    }

    EditorProfiler profiler;
    renderer::metrics::FrameTimingReport report;
    report.add_pass("forward_opaque", 1000U, 1U, false);
    report.total_instances = 10U;
    report.visible_instances = 6U;
    report.culled_instances = 4U;
    profiler.set_startup_nanoseconds(500U);
    profiler.update_frame(report);
    profiler.sample_memory();
    if (!expect(profiler.has_frame()) || !expect(profiler.frame().pass_count == 1U) ||
        !expect(profiler.frame().visible_instances == 6U) ||
        !expect(profiler.startup_nanoseconds() == 500U)) {
        std::filesystem::remove_all(directory, error);
        return 8;
    }

    UiState ui;
    std::vector<UiVertex> vertices;
    ui.build(project, catalog, console, profiler, 1280U, 720U, vertices);
    if (!expect(!vertices.empty()) || !expect(ui.active_panel() == EditorPanel::assets)) {
        std::filesystem::remove_all(directory, error);
        return 9;
    }
    const core::f32 panel_top = 720.0F - ui.layout().bottom_panel_height;
    if (!expect(ui.click(project, catalog, 300.0F, panel_top + 10.0F, 1280U, 720U).ok()) ||
        !expect(ui.active_panel() == EditorPanel::assets) ||
        !expect(ui.click(project, catalog, 500.0F, panel_top + 42.0F, 1280U, 720U).ok()) ||
        !expect(catalog.selected() != nullptr)) {
        std::filesystem::remove_all(directory, error);
        return 10;
    }

    std::filesystem::remove_all(directory, error);
    return 0;
}
