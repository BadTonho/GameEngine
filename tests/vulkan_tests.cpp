#include "engine/platform/platform.hpp"
#include "engine/rhi/rhi.hpp"

#include "engine/assets/prepared_assets.hpp"
#include "engine/renderer/gpu_culling.hpp"
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/renderer_quality.hpp"
#include "engine/renderer/scene_bridge.hpp"
#include "engine/scene/scene_serialization.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif

namespace {

[[nodiscard]] std::FILE* open_file(const char* path, const char* mode) noexcept
{
#if defined(_WIN32)
    std::FILE* file = nullptr;
    return fopen_s(&file, path, mode) == 0 ? file : nullptr;
#else
    return std::fopen(path, mode);
#endif
}

void append_vertex_f32(std::vector<std::byte>& bytes, float value)
{
    const gameengine::core::u32 bits = std::bit_cast<gameengine::core::u32>(value);
    for (std::size_t i = 0; i < 4U; ++i) {
        bytes.push_back(std::byte{static_cast<unsigned char>((bits >> (i * 8U)) & 0xffU)});
    }
}

void append_index_u16(std::vector<std::byte>& bytes, std::uint16_t value)
{
    bytes.push_back(std::byte{static_cast<unsigned char>(value & 0xffU)});
    bytes.push_back(std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)});
}

void append_index_u32(std::vector<std::byte>& bytes, gameengine::core::u32 value)
{
    for (std::size_t i = 0; i < 4U; ++i) {
        bytes.push_back(std::byte{static_cast<unsigned char>((value >> (i * 8U)) & 0xffU)});
    }
}

gameengine::assets::PreparedMesh make_test_mesh(gameengine::core::u64 id, bool use_u32)
{
    gameengine::assets::PreparedMesh mesh{};
    mesh.asset_id = id;
    mesh.content_hash = id ^ 0x1234ULL;
    mesh.vertex_count = 4U;
    mesh.vertex_stride = 32U;
    mesh.index_format = use_u32 ? gameengine::assets::MeshIndexFormat::u32
                                : gameengine::assets::MeshIndexFormat::u16;
    mesh.index_count = use_u32 ? 6U : 3U;
    for (const float value : {
             -0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F,
              0.5F, -0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F,
              0.5F,  0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F,
             -0.5F,  0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}) {
        append_vertex_f32(mesh.vertices, value);
    }
    if (use_u32) {
        for (const gameengine::core::u32 idx : {0U, 1U, 2U, 2U, 3U, 0U}) {
            append_index_u32(mesh.indices, idx);
        }
        mesh.submeshes = {
            {.first_index = 0U, .index_count = 3U, .material_slot = 0U, .vertex_offset = 0U},
            {.first_index = 3U, .index_count = 3U, .material_slot = 1U, .vertex_offset = 0U},
        };
    } else {
        for (const std::uint16_t idx : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{2}}) {
            append_index_u16(mesh.indices, idx);
        }
        mesh.submeshes = {
            {.first_index = 0U, .index_count = 3U, .material_slot = 0U, .vertex_offset = 0U},
        };
    }
    return mesh;
}

} // namespace

int main()
{
    constexpr const char* cache_path = "gameengine-vulkan-test.pipeline.cache";
    std::remove(cache_path);
    std::remove("gameengine-vulkan-test.pipeline.cache.tmp");

    gameengine::platform::Platform platform;
    if (!platform.initialize().ok()) {
        return 1;
    }

    if (!platform.create_window({
            .title = "GameEngine Vulkan resize test",
            .width = 320,
            .height = 240,
            .resizable = true,
        })) {
        return 2;
    }

    std::array<char, 513> oversized_cache_path{};
    oversized_cache_path.fill('x');
    oversized_cache_path.back() = '\0';
    gameengine::rhi::Renderer invalid_path_renderer;
    const gameengine::rhi::RendererConfiguration invalid_path_configuration{
        .pipeline_cache_path = oversized_cache_path.data(),
        .enable_shader_hot_reload = false,
    };
    if (invalid_path_renderer.initialize(platform, invalid_path_configuration).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 3;
    }

    gameengine::rhi::Renderer renderer;
    const gameengine::rhi::RendererConfiguration cache_configuration{
        .pipeline_cache_path = cache_path,
        .enable_shader_hot_reload = false,
    };
    if (!renderer.initialize(platform, cache_configuration).ok()) {
        return 4;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 5;
    }

    gameengine::rhi::BufferHandle buffer;
    if (renderer.create_buffer({.size = 0}, buffer).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 6;
    }
    if (!renderer.create_buffer({.size = 16}, buffer).ok() || !buffer.valid()) {
        return 7;
    }
    const std::array<std::byte, 16> buffer_data{};
    if (!renderer.upload_buffer(buffer, buffer_data).ok()) {
        return 8;
    }

    gameengine::rhi::ImageHandle image;
    if (renderer.create_image({.width = 0, .height = 1}, image).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 9;
    }
    if (!renderer.create_image({.width = 1, .height = 1}, image).ok() || !image.valid()) {
        return 10;
    }
    const std::array<std::byte, 4> pixel = {
        std::byte{0xff},
        std::byte{0x00},
        std::byte{0xff},
        std::byte{0xff},
    };
    if (!renderer.upload_image(image, pixel).ok()) {
        return 11;
    }

    gameengine::rhi::SamplerHandle sampler;
    if (!renderer.create_sampler({}, sampler).ok() || !sampler.valid()) {
        return 12;
    }

    gameengine::rhi::PipelineHandle pipeline;
    if (!renderer.create_graphics_pipeline({}, pipeline).ok() || !pipeline.valid()) {
        return 13;
    }
    if (!renderer.destroy_buffer(buffer).ok() ||
        renderer.destroy_buffer(buffer).code != gameengine::core::ErrorCode::invalid_argument ||
        !renderer.destroy_image(image).ok() || !renderer.destroy_sampler(sampler).ok() ||
        !renderer.destroy_pipeline(pipeline).ok()) {
        return 14;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 15;
    }
    if (renderer.upload_buffer(buffer, buffer_data).code !=
        gameengine::core::ErrorCode::invalid_argument) {
        return 16;
    }

    const auto handles = platform.native_window_handles();
#if defined(_WIN32)
    const auto hwnd = reinterpret_cast<HWND>(handles.window);
    RECT rect{0, 0, 640, 480};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    platform.poll_events();
#else
    auto* display = reinterpret_cast<Display*>(handles.display);
    const auto window = static_cast<::Window>(handles.window);
    XResizeWindow(display, window, 640, 480);
    XFlush(display);
    XSync(display, False);
    platform.poll_events();
#endif

    const auto size = platform.window_size();
    if (size.width != 640 || size.height != 480) {
        return 17;
    }
    if (!renderer.render_frame(platform).ok()) {
        return 18;
    }
    if (renderer.reload_shaders().code !=
        gameengine::core::ErrorCode::shader_reload_disabled) {
        renderer.shutdown();
        std::remove(cache_path);
        return 19;
    }

    renderer.shutdown();
    std::FILE* cache_file = open_file(cache_path, "rb");
    const bool cache_closed = cache_file != nullptr && std::fclose(cache_file) == 0;
    if (!cache_closed) {
        std::remove(cache_path);
        return 20;
    }

    std::FILE* incompatible_file = open_file(cache_path, "r+b");
    const bool incompatible_written = incompatible_file != nullptr &&
                                      std::fseek(incompatible_file, 0, SEEK_SET) == 0 &&
                                      std::fwrite("BAD", 3, 1, incompatible_file) == 1;
    const bool incompatible_closed = incompatible_file != nullptr &&
                                     std::fclose(incompatible_file) == 0;
    if (!incompatible_written || !incompatible_closed) {
        std::remove(cache_path);
        return 21;
    }
    gameengine::rhi::Renderer incompatible_renderer;
    if (!incompatible_renderer.initialize(platform, cache_configuration).ok() ||
        !incompatible_renderer.render_frame(platform).ok()) {
        incompatible_renderer.shutdown();
        std::remove(cache_path);
        return 22;
    }
    incompatible_renderer.shutdown();

    gameengine::rhi::Renderer cached_renderer;
    if (!cached_renderer.initialize(platform, cache_configuration).ok() ||
        !cached_renderer.render_frame(platform).ok()) {
        cached_renderer.shutdown();
        std::remove(cache_path);
        return 23;
    }
    cached_renderer.shutdown();

    gameengine::rhi::Renderer reload_renderer;
    const gameengine::rhi::RendererConfiguration reload_configuration{
        .pipeline_cache_path = cache_path,
        .enable_shader_hot_reload = true,
    };
#ifdef NDEBUG
    if (reload_renderer.initialize(platform, reload_configuration).code !=
        gameengine::core::ErrorCode::shader_reload_disabled) {
        reload_renderer.shutdown();
        std::remove(cache_path);
        return 24;
    }
#else
    if (!reload_renderer.initialize(platform, reload_configuration).ok() ||
        !reload_renderer.reload_shaders().ok() ||
        !reload_renderer.render_frame(platform).ok()) {
        reload_renderer.shutdown();
        std::remove(cache_path);
        return 24;
    }
#endif
    reload_renderer.shutdown();

    std::FILE* malformed = open_file(cache_path, "wb");
    const bool malformed_written = malformed != nullptr &&
                                   std::fwrite("bad", 3, 1, malformed) == 1;
    const bool malformed_closed = malformed != nullptr && std::fclose(malformed) == 0;
    if (!malformed_written || !malformed_closed) {
        std::remove(cache_path);
        return 25;
    }
    gameengine::rhi::Renderer invalid_cache_renderer;
    if (!invalid_cache_renderer.initialize(platform, cache_configuration).ok() ||
        !invalid_cache_renderer.render_frame(platform).ok()) {
        invalid_cache_renderer.shutdown();
        std::remove(cache_path);
        return 26;
    }
    invalid_cache_renderer.shutdown();
    std::remove(cache_path);
    std::remove("gameengine-vulkan-test.pipeline.cache.tmp");

    {
        gameengine::rhi::Renderer prepared_renderer;
        if (!prepared_renderer.initialize(platform, cache_configuration).ok()) {
            platform.shutdown();
            return 27;
        }

        const gameengine::assets::PreparedTexture texture{
            .asset_id = 101ULL,
            .content_hash = 101ULL,
            .width = 2U,
            .height = 2U,
            .mip_count = 1U,
            .format = 1U,
            .pixels = std::vector<std::byte>(16U, std::byte{255}),
        };
        const gameengine::assets::PreparedMesh mesh_u16 = make_test_mesh(201ULL, false);
        const gameengine::assets::PreparedMesh mesh_u32 = make_test_mesh(202ULL, true);
        const gameengine::assets::PreparedMaterial textured_material{
            .asset_id = 301ULL,
            .content_hash = 301ULL,
            .base_color_factor = {1.0F, 0.8F, 0.6F, 1.0F},
            .metallic = 0.2F,
            .roughness = 0.45F,
            .albedo_texture = texture.asset_id,
        };
        const gameengine::assets::PreparedMaterial untextured_material{
            .asset_id = 302ULL,
            .content_hash = 302ULL,
            .base_color_factor = {0.3F, 0.7F, 0.9F, 1.0F},
            .metallic = 0.1F,
            .roughness = 0.75F,
            .albedo_texture = 0U,
        };

        // Invalid missing texture ID must fail cleanly without poisoning renderer state.
        const std::array bad_materials{
            gameengine::assets::PreparedMaterial{
                .asset_id = 303ULL,
                .content_hash = 303ULL,
                .albedo_texture = 999999ULL,
            },
        };
        if (gameengine::renderer::scene_bridge::set_prepared_assets(
                prepared_renderer,
                {.materials = bad_materials})
                .code != gameengine::core::ErrorCode::invalid_argument) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 28;
        }

        // Invalid vertex stride must fail cleanly without poisoning renderer state.
        gameengine::assets::PreparedMesh bad_stride_mesh = mesh_u16;
        bad_stride_mesh.vertex_stride = 16U;
        const std::array bad_meshes{bad_stride_mesh};
        if (gameengine::renderer::scene_bridge::set_prepared_assets(
                prepared_renderer,
                {.meshes = bad_meshes})
                .code != gameengine::core::ErrorCode::invalid_argument) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 29;
        }

        const std::array valid_textures{texture};
        const std::array valid_meshes{mesh_u16, mesh_u32};
        const std::array valid_materials{textured_material, untextured_material};
        const gameengine::renderer::scene_bridge::PreparedRenderAssets valid_assets{
            .meshes = valid_meshes,
            .textures = valid_textures,
            .materials = valid_materials,
        };

        gameengine::scene::Scene multi_scene;
        const auto entity_u16 = multi_scene.create_entity();
        const auto entity_u32 = multi_scene.create_entity();
        const auto entity_bootstrap = multi_scene.create_entity();
        if (!multi_scene.add_transform(entity_u16).ok() ||
            !multi_scene
                 .add_mesh_renderer(entity_u16, {mesh_u16.asset_id, textured_material.asset_id})
                 .ok() ||
            !multi_scene.add_transform(entity_u32).ok() ||
            !multi_scene
                 .add_mesh_renderer(entity_u32, {mesh_u32.asset_id, untextured_material.asset_id})
                 .ok() ||
            !multi_scene.add_transform(entity_bootstrap).ok() ||
            !multi_scene
                 .add_mesh_renderer(entity_bootstrap,
                                    {gameengine::scene::bootstrap_mesh_id,
                                     gameengine::scene::bootstrap_material_id})
                 .ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 30;
        }
        multi_scene.transform(entity_u32)->local_position = {1.5F, 0.0F, 0.0F};
        multi_scene.transform(entity_bootstrap)->local_position = {-1.5F, 0.0F, 0.0F};
        if (!gameengine::scene::ensure_default_camera_and_light(multi_scene).ok() ||
            !multi_scene.update_transforms().ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 31;
        }

        if (!gameengine::renderer::scene_bridge::set_prepared_assets(prepared_renderer, valid_assets)
                 .ok() ||
            !gameengine::renderer::scene_bridge::attach_scene(prepared_renderer, multi_scene).ok() ||
            !prepared_renderer.render_frame(platform).ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 32;
        }

        for (const auto quality_level : {
                 gameengine::renderer::quality::RendererQuality::low,
                 gameengine::renderer::quality::RendererQuality::high,
                 gameengine::renderer::quality::RendererQuality::medium,
             }) {
            if (!gameengine::renderer::diagnostics::set_renderer_quality(prepared_renderer,
                                                                         quality_level)
                     .ok() ||
                !prepared_renderer.render_frame(platform).ok()) {
                prepared_renderer.shutdown();
                platform.shutdown();
                return 33;
            }
        }

#if defined(_WIN32)
        RECT resized_rect{0, 0, 480, 360};
        AdjustWindowRect(&resized_rect, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(reinterpret_cast<HWND>(handles.window),
                     nullptr,
                     0,
                     0,
                     resized_rect.right - resized_rect.left,
                     resized_rect.bottom - resized_rect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        platform.poll_events();
#else
        XResizeWindow(display, window, 480, 360);
        XFlush(display);
        XSync(display, False);
        platform.poll_events();
#endif
        if (!prepared_renderer.render_frame(platform).ok() ||
            !gameengine::renderer::scene_bridge::detach_scene(prepared_renderer).ok() ||
            !gameengine::renderer::scene_bridge::clear_prepared_assets(prepared_renderer).ok() ||
            !prepared_renderer.render_frame(platform).ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 34;
        }

        gameengine::renderer::metrics::FrameTimingReport low_report;
        if (!gameengine::renderer::diagnostics::set_visibility_mode(
                 prepared_renderer,
                 gameengine::renderer::gpu_culling::VisibilityMode::cpu)
                 .ok() ||
            !gameengine::renderer::diagnostics::set_renderer_quality(
                 prepared_renderer,
                 gameengine::renderer::quality::RendererQuality::low)
                 .ok() ||
            !prepared_renderer.render_frame(platform).ok() ||
            !gameengine::editor::renderer_bridge::read_metrics(prepared_renderer, low_report)
                 .ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 35;
        }
        if (low_report.light_buffer_bytes != 0U || low_report.tile_header_buffer_bytes != 0U ||
            low_report.tile_index_buffer_bytes != 0U || low_report.shadow_map_bytes != 0U ||
            low_report.environment_bytes != 0U || low_report.gpu_source_buffer_bytes != 0U ||
            low_report.gpu_visible_buffer_bytes != 0U ||
            low_report.vram.transient_staging_bytes != 0U ||
            low_report.vram.transient_staging_peak_bytes == 0U ||
            low_report.vram.total_allocated_bytes == 0U) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 36;
        }

        gameengine::renderer::metrics::FrameTimingReport high_report;
        if (!gameengine::renderer::diagnostics::set_renderer_quality(
                 prepared_renderer,
                 gameengine::renderer::quality::RendererQuality::high)
                 .ok() ||
            !prepared_renderer.render_frame(platform).ok() ||
            !gameengine::editor::renderer_bridge::read_metrics(prepared_renderer, high_report)
                 .ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 37;
        }
        if (high_report.vram.total_allocated_bytes <= low_report.vram.total_allocated_bytes) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 38;
        }

        if (!gameengine::renderer::diagnostics::set_renderer_quality(
                 prepared_renderer,
                 gameengine::renderer::quality::RendererQuality::low)
                 .ok() ||
            !prepared_renderer.render_frame(platform).ok() ||
            !gameengine::editor::renderer_bridge::read_metrics(prepared_renderer, low_report)
                 .ok() ||
            low_report.light_buffer_bytes != 0U || low_report.shadow_map_bytes != 0U ||
            low_report.environment_bytes != 0U) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 39;
        }

        if (!gameengine::renderer::diagnostics::set_vram_budget(prepared_renderer, 1U).ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 40;
        }
        gameengine::rhi::BufferHandle budget_buffer{};
        const auto budget_fail = prepared_renderer.create_buffer(
            {.size = 256U, .usage = gameengine::rhi::BufferUsage::vertex},
            budget_buffer);
        if (budget_fail.code != gameengine::core::ErrorCode::allocation_failed ||
            budget_buffer.valid()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 41;
        }
        if (!gameengine::renderer::diagnostics::set_vram_budget(prepared_renderer, 0U).ok() ||
            !prepared_renderer.render_frame(platform).ok()) {
            prepared_renderer.shutdown();
            platform.shutdown();
            return 42;
        }

        prepared_renderer.shutdown();
        std::remove(cache_path);
        std::remove("gameengine-vulkan-test.pipeline.cache.tmp");
    }

    platform.shutdown();
    return 0;
}
