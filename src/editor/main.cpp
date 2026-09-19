#include "engine/core/core.hpp"
#include "engine/core/diagnostics.hpp"
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
#include <vector>

namespace {

struct Arguments final {
    std::filesystem::path project_path{};
    std::filesystem::path new_directory{};
    bool smoke_test = false;
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
        } else {
            return false;
        }
    }
    return (!arguments.project_path.empty() && arguments.new_directory.empty()) ||
           (!arguments.new_directory.empty() && arguments.project_path.empty());
}

void print_usage() noexcept
{
    gameengine::core::log(gameengine::core::LogLevel::info,
                          "usage: gameengine_editor --project <file.geproject> [--smoke-test] or "
                          "--new-project <directory>");
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
    profiler.set_startup_nanoseconds(static_cast<gameengine::core::u64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - editor_start)
            .count()));
    gameengine::editor::UiState ui;
    std::vector<gameengine::editor::UiVertex> ui_vertices;
    ui_vertices.reserve(16'384);
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
        ui.build(project, catalog, console, profiler, size.width, size.height, ui_vertices);
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
        const gameengine::core::f32 delta = 0.02F;
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
    core.shutdown();
    return 0;
}
