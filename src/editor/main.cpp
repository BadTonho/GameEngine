#include "engine/core/core.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/diagnostics.hpp"
#if GAMEENGINE_BUILD_AUDIO
#include "engine/audio/audio.hpp"
#endif
#if GAMEENGINE_BUILD_ANIMATION
#include "engine/animation/animation.hpp"
#endif
#include "engine/editor/asset_catalog.hpp"
#include "engine/editor/console.hpp"
#include "engine/editor/profiler.hpp"
#include "engine/editor/project.hpp"
#include "engine/editor/renderer_bridge.hpp"
#include "engine/editor/ui.hpp"
#include "engine/platform/platform.hpp"
#include "engine/renderer/renderer_quality.hpp"
#include "engine/rhi/rhi.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

struct Arguments final {
    std::filesystem::path project_path{};
    std::filesystem::path new_directory{};
    bool smoke_test = false;
    bool audio_smoke_test = false;
};

struct EditorEvents final {
    bool left_click = false;
    gameengine::core::i32 mouse_x = 0;
    gameengine::core::i32 mouse_y = 0;
};

void editor_event_callback(const gameengine::input::Event& event, void* user_data) noexcept
{
    auto* events = static_cast<EditorEvents*>(user_data);
    if (events != nullptr && event.type == gameengine::input::EventType::mouse_button_pressed &&
        event.mouse_button == gameengine::input::MouseButton::left) {
        events->left_click = true;
        events->mouse_x = event.x;
        events->mouse_y = event.y;
    }
}

[[nodiscard]] bool parse_arguments(int argc, char** argv, Arguments& arguments) noexcept
{
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--project") == 0 && index + 1 < argc) {
            arguments.project_path = argv[++index];
        } else if (std::strcmp(argv[index], "--new-project") == 0 && index + 1 < argc) {
            arguments.new_directory = argv[++index];
        } else if (std::strcmp(argv[index], "--smoke-test") == 0) {
            arguments.smoke_test = true;
        } else if (std::strcmp(argv[index], "--audio-smoke-test") == 0) {
            arguments.audio_smoke_test = true;
        } else {
            return false;
        }
    }
    const bool project_mode = !arguments.project_path.empty() && arguments.new_directory.empty();
    const bool new_project_mode = !arguments.new_directory.empty() && arguments.project_path.empty();
    return (project_mode || new_project_mode) &&
           !(arguments.smoke_test && arguments.audio_smoke_test);
}

void print_usage() noexcept
{
    gameengine::core::log(gameengine::core::LogLevel::info,
                          "usage: gameengine_editor --project <file.geproject> [--smoke-test] or "
                          "--new-project <directory> [--audio-smoke-test]");
}

} // namespace

int main(int argc, char** argv)
{
    Arguments arguments;
    if (!parse_arguments(argc, argv, arguments)) {
        print_usage();
        return 2;
    }

    gameengine::core::Core core;
    if (!core.initialize()) {
        return 1;
    }

    gameengine::editor::ProjectSession project;
    gameengine::core::Status project_status;
    if (!arguments.new_directory.empty()) {
        project_status = project.create_new(arguments.new_directory);
    } else {
        project_status = project.load(arguments.project_path);
    }
    if (!project_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(project_status.code));
        core.shutdown();
        return 3;
    }

    gameengine::editor::ConsoleBuffer console;
    gameengine::editor::record_console_message(
        console, gameengine::core::LogLevel::info, "PROJECT READY");
#if GAMEENGINE_BUILD_AUDIO
    gameengine::audio::AudioSystem audio;
    const gameengine::core::Status audio_status = audio.initialize();
    gameengine::editor::record_console_status(
        console,
        audio_status ? gameengine::core::LogLevel::info : gameengine::core::LogLevel::error,
        audio_status ? (audio.available() ? "AUDIO BACKEND READY" : "AUDIO UNAVAILABLE")
                     : "AUDIO INITIALIZE FAILED",
        audio_status);
    if (arguments.audio_smoke_test) {
        if (!audio_status || !audio.available()) {
            gameengine::core::log(gameengine::core::LogLevel::info,
                                  "editor audio: unavailable");
            audio.shutdown();
            core.shutdown();
            return 0;
        }
        gameengine::audio::VoiceHandle voice{};
        const gameengine::core::Status play_status = audio.play_tone(
            {.frequency_hz = 440.0F,
             .duration_seconds = 0.25F,
             .gain = 0.15F,
             .pan = 0.0F,
             .pitch = 1.0F,
             .loop = false},
            voice);
        if (!play_status) {
            audio.shutdown();
            core.shutdown();
            return 6;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        static_cast<void>(audio.stop(voice));
        const gameengine::audio::AudioMetrics metrics = audio.metrics();
        std::fprintf(stdout,
                     "editor audio backend=%.*s frames=%llu underruns=%llu\n",
                     static_cast<int>(metrics.backend.size()),
                     metrics.backend.data(),
                     static_cast<unsigned long long>(metrics.frames_played),
                     static_cast<unsigned long long>(metrics.underruns));
        audio.shutdown();
        core.shutdown();
        return 0;
    }
#else
    if (arguments.audio_smoke_test) {
        gameengine::core::log(gameengine::core::LogLevel::info,
                              "editor audio: unavailable (audio module disabled)");
        core.shutdown();
        return 0;
    }
#endif
    gameengine::editor::AssetCatalog catalog;
    const gameengine::core::Status catalog_status =
        catalog.refresh(project.project_path().parent_path());
    gameengine::editor::record_console_status(console,
                                              catalog_status
                                                  ? gameengine::core::LogLevel::info
                                                  : gameengine::core::LogLevel::error,
                                              catalog_status ? "ASSET CATALOG READY"
                                                             : "ASSET CATALOG FAILED",
                                              catalog_status);
    gameengine::editor::EditorProfiler profiler;
    const auto editor_start = std::chrono::steady_clock::now();
    profiler.sample_memory();

#if GAMEENGINE_RENDERER_HAS_VULKAN
    gameengine::platform::Platform platform;
    if (!platform.initialize()) {
        core.shutdown();
        return 4;
    }
    const gameengine::platform::WindowDescription window_description{
        .title = "GameEngine Editor",
        .width = project.manifest().window_width,
        .height = project.manifest().window_height,
        .resizable = true,
    };
    if (!platform.create_window(window_description)) {
        platform.shutdown();
        core.shutdown();
        return 5;
    }

    gameengine::rhi::Renderer renderer;
    const gameengine::core::Status renderer_status = renderer.initialize(platform);
    if (!renderer_status) {
        gameengine::editor::record_console_status(
            console, gameengine::core::LogLevel::error, "RENDERER INITIALIZE FAILED", renderer_status);
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }
    const gameengine::core::Status quality_status =
        gameengine::renderer::diagnostics::set_renderer_quality(
            renderer, project.manifest().renderer_quality);
    if (!quality_status) {
        gameengine::editor::record_console_status(
            console, gameengine::core::LogLevel::error, "RENDERER QUALITY FAILED", quality_status);
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }
    const gameengine::core::Status scene_status =
        gameengine::editor::renderer_bridge::attach_scene(renderer, project.scene());
    if (!scene_status) {
        gameengine::editor::record_console_status(
            console, gameengine::core::LogLevel::error, "SCENE ATTACH FAILED", scene_status);
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }
#if GAMEENGINE_BUILD_ANIMATION
    gameengine::animation::AnimationSystem animation_system;
    gameengine::scene::Entity animation_entity{};
    for (const gameengine::scene::Entity entity : project.scene().entities()) {
        if (project.scene().mesh_renderer(entity) != nullptr) {
            animation_entity = entity;
            break;
        }
    }
    const gameengine::scene::TransformComponent* animation_transform =
        project.scene().transform(animation_entity);
    if (!animation_entity.valid() || animation_transform == nullptr ||
        !animation_system.reserve_players(1U) ||
        !animation_system.add_procedural_player(animation_entity, *animation_transform)) {
        gameengine::editor::record_console_message(
            console, gameengine::core::LogLevel::error, "ANIMATION INITIALIZE FAILED");
        static_cast<void>(gameengine::editor::renderer_bridge::detach_scene(renderer));
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }
#endif
    profiler.set_startup_nanoseconds(static_cast<gameengine::core::u64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - editor_start)
            .count()));
    gameengine::editor::UiState ui;
    std::vector<gameengine::editor::UiVertex> ui_vertices;
    ui_vertices.reserve(16'384);
    gameengine::core::Clock clock;
    gameengine::editor::AnimationUiState animation_ui_state{};
    bool quality_fallback_reported = false;
    const auto update_ui = [&]() noexcept -> gameengine::core::Status {
        const auto size = platform.window_size();
        gameengine::renderer::metrics::FrameTimingReport report;
        if (gameengine::editor::renderer_bridge::read_metrics(renderer, report)) {
            profiler.update_frame(report);
            if (!quality_fallback_reported &&
                (report.quality_compute_fallback || report.quality_shadow_fallback ||
                 report.quality_environment_fallback)) {
                gameengine::editor::record_console_message(
                    console,
                    gameengine::core::LogLevel::warning,
                    "RENDERER QUALITY FALLBACK ACTIVE");
                quality_fallback_reported = true;
            }
        }
#if GAMEENGINE_BUILD_ANIMATION
        animation_ui_state = {};
        if (const auto* player = animation_system.player(animation_entity);
            player != nullptr && !player->clip.name.empty()) {
            animation_ui_state.available = true;
            animation_ui_state.playing = player->playing;
            animation_ui_state.clip_name = player->clip.name;
            animation_ui_state.time_seconds = player->time_seconds;
            animation_ui_state.duration_seconds = player->clip.duration_seconds;
        }
        ui.build(project,
                 catalog,
                 console,
                 profiler,
                 animation_ui_state,
                 size.width,
                 size.height,
                 ui_vertices);
#else
        ui.build(project, catalog, console, profiler, size.width, size.height, ui_vertices);
#endif
        const gameengine::core::Status status =
            gameengine::editor::renderer_bridge::set_ui_vertices(renderer, ui_vertices);
        if (!status) {
            gameengine::editor::record_console_status(
                console, gameengine::core::LogLevel::error, "UI UPDATE FAILED", status);
        }
        return status;
    };
    if (!update_ui()) {
        static_cast<void>(gameengine::editor::renderer_bridge::detach_scene(renderer));
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 7;
    }
    if (arguments.smoke_test) {
#if GAMEENGINE_BUILD_ANIMATION
        const gameengine::core::Status animation_status = animation_system.update(project.scene(), 0.0F);
        if (!animation_status) {
            gameengine::editor::record_console_message(
                console, gameengine::core::LogLevel::error, "ANIMATION UPDATE FAILED");
        }
#endif
        const gameengine::core::Status frame_status = renderer.render_frame(platform);
        if (!frame_status) {
            gameengine::editor::record_console_status(
                console, gameengine::core::LogLevel::error, "EDITOR FRAME FAILED", frame_status);
        }
        static_cast<void>(gameengine::editor::renderer_bridge::detach_scene(renderer));
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return frame_status ? 0 : 8;
    }

    bool previous_save_down = false;
    gameengine::core::u64 frame_counter = 0;
    while (!platform.should_close()) {
        EditorEvents events;
        platform.poll_events(editor_event_callback, &events);
        const auto& input = platform.input();
        if (events.left_click) {
            static_cast<void>(ui.click(
                project,
                catalog,
                static_cast<gameengine::core::f32>(events.mouse_x),
                static_cast<gameengine::core::f32>(events.mouse_y),
                platform.window_size().width,
                platform.window_size().height));
        }
        if (ui.consume_refresh_request()) {
            const gameengine::core::Status refresh_status =
                catalog.refresh(project.project_path().parent_path());
            gameengine::editor::record_console_status(
                console,
                refresh_status ? gameengine::core::LogLevel::info
                               : gameengine::core::LogLevel::error,
                refresh_status ? "ASSET CATALOG REFRESHED" : "ASSET CATALOG REFRESH FAILED",
                refresh_status);
        }
        if (input.is_key_down(gameengine::input::KeyCode::escape)) {
            platform.request_close();
        }
        const bool save_down = input.is_key_down(gameengine::input::KeyCode::control) &&
                               input.is_key_down(gameengine::input::KeyCode::s);
        if (save_down && !previous_save_down) {
#if GAMEENGINE_BUILD_ANIMATION
            static_cast<void>(animation_system.pause(animation_entity));
#endif
            const gameengine::core::Status save_status = project.save();
            gameengine::editor::record_console_status(
                console,
                save_status ? gameengine::core::LogLevel::info
                            : gameengine::core::LogLevel::error,
                save_status ? "SCENE SAVED" : "SCENE SAVE FAILED",
                save_status);
        }
        previous_save_down = save_down;
        if ((frame_counter++ % 60U) == 0U) {
            profiler.sample_memory();
        }
        const gameengine::core::f32 delta = static_cast<gameengine::core::f32>(clock.tick());
        bool transform_edit = false;
        transform_edit = transform_edit || input.is_key_down(gameengine::input::KeyCode::left);
        transform_edit = transform_edit || input.is_key_down(gameengine::input::KeyCode::right);
        transform_edit = transform_edit || input.is_key_down(gameengine::input::KeyCode::up);
        transform_edit = transform_edit || input.is_key_down(gameengine::input::KeyCode::down);
        transform_edit = transform_edit || input.is_key_down(gameengine::input::KeyCode::page_up);
        transform_edit = transform_edit || input.is_key_down(gameengine::input::KeyCode::page_down);
        transform_edit = transform_edit || input.is_key_down(gameengine::input::KeyCode::r);
#if GAMEENGINE_BUILD_ANIMATION
        if (transform_edit) {
            static_cast<void>(animation_system.pause(animation_entity));
        }
#endif
#if GAMEENGINE_BUILD_AUDIO
        const gameengine::core::Status audio_update_status = audio.update(delta);
        if (!audio_update_status) {
            gameengine::editor::record_console_status(
                console,
                gameengine::core::LogLevel::error,
                "AUDIO UPDATE FAILED",
                audio_update_status);
            platform.request_close();
        }
#endif
        if (input.is_key_down(gameengine::input::KeyCode::left)) {
            static_cast<void>(project.move_selected(-delta, 0.0F, 0.0F));
        }
        if (input.is_key_down(gameengine::input::KeyCode::right)) {
            static_cast<void>(project.move_selected(delta, 0.0F, 0.0F));
        }
        if (input.is_key_down(gameengine::input::KeyCode::up)) {
            static_cast<void>(project.move_selected(0.0F, 0.0F, -delta));
        }
        if (input.is_key_down(gameengine::input::KeyCode::down)) {
            static_cast<void>(project.move_selected(0.0F, 0.0F, delta));
        }
        if (input.is_key_down(gameengine::input::KeyCode::page_up)) {
            static_cast<void>(project.move_selected(0.0F, delta, 0.0F));
        }
        if (input.is_key_down(gameengine::input::KeyCode::page_down)) {
            static_cast<void>(project.move_selected(0.0F, -delta, 0.0F));
        }
        if (input.is_key_down(gameengine::input::KeyCode::r)) {
            static_cast<void>(project.reset_selected());
        }
#if GAMEENGINE_BUILD_ANIMATION
        const gameengine::core::Status animation_status = animation_system.update(
            project.scene(), delta);
        if (!animation_status) {
            gameengine::editor::record_console_message(
                console, gameengine::core::LogLevel::error, "ANIMATION UPDATE FAILED");
            platform.request_close();
        }
#endif
        if (!update_ui()) {
            platform.request_close();
            continue;
        }
        if (!renderer.render_frame(platform)) {
            gameengine::editor::record_console_message(
                console, gameengine::core::LogLevel::error, "EDITOR FRAME FAILED");
            platform.request_close();
        }
    }
    static_cast<void>(gameengine::editor::renderer_bridge::detach_scene(renderer));
    renderer.shutdown();
#else
    gameengine::core::log(gameengine::core::LogLevel::info,
                          "editor viewport: unavailable (Vulkan disabled)");
    static_cast<void>(arguments);
#endif

#if GAMEENGINE_RENDERER_HAS_VULKAN
    platform.shutdown();
#endif
#if GAMEENGINE_BUILD_AUDIO
    audio.shutdown();
#endif
    core.shutdown();
    return 0;
}
