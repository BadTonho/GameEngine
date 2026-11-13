#include "engine/assets/prepared_assets.hpp"
#include "engine/core/core.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/diagnostics.hpp"
#if GAMEENGINE_BUILD_AUDIO
#include "engine/audio/audio.hpp"
#endif
#include "engine/platform/platform.hpp"
#include "engine/renderer/scene_bridge.hpp"
#include "engine/scene/scene.hpp"
#include "engine/scene/scene_serialization.hpp"
#if GAMEENGINE_BUILD_ANIMATION
#include "engine/animation/animation.hpp"
#endif
#if GAMEENGINE_BUILD_PHYSICS
#include "engine/physics/physics.hpp"
#endif
#if GAMEENGINE_BUILD_LUA
#include "engine/scripting/scripting.hpp"
#endif
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/renderer_quality.hpp"
#include "engine/rhi/rhi.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <vector>

#if GAMEENGINE_BUILD_AUDIO
namespace {

int run_audio_smoke_test() noexcept
{
    gameengine::audio::AudioSystem audio;
    const gameengine::core::Status initialize_status = audio.initialize();
    if (!initialize_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(initialize_status.code));
        return 1;
    }
    if (!audio.available()) {
        gameengine::core::log(gameengine::core::LogLevel::info,
                              "audio: unavailable (stub backend or no output device)");
        audio.shutdown();
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
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(play_status.code));
        audio.shutdown();
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    static_cast<void>(audio.stop(voice));
    const gameengine::audio::AudioMetrics metrics = audio.metrics();
    std::fprintf(stdout,
                 "audio backend=%.*s available=%s frames=%llu underruns=%llu\n",
                 static_cast<int>(metrics.backend.size()),
                 metrics.backend.data(),
                 metrics.available ? "true" : "false",
                 static_cast<unsigned long long>(metrics.frames_played),
                 static_cast<unsigned long long>(metrics.underruns));
    audio.shutdown();
    return 0;
}

} // namespace
#endif

#if GAMEENGINE_BUILD_LUA
namespace {

int run_lua_smoke_test() noexcept
{
    gameengine::scene::Scene scene;
    const gameengine::scene::Entity cube = scene.create_entity();
    if (!cube.valid() ||
        !scene.add_transform(cube, {.local_position = {0.0F, 0.0F, 0.0F}}) ||
        !scene.update_transforms()) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "lua smoke scene creation failed");
        return 1;
    }

    gameengine::scripting::ScriptRuntime script_runtime;
    const gameengine::core::Status init_status = script_runtime.initialize();
    if (!init_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(init_status.code));
        return 1;
    }

    const gameengine::core::Status reg_status =
        script_runtime.register_entity("BootstrapCube", cube);
    if (!reg_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(reg_status.code));
        script_runtime.shutdown();
        return 1;
    }

    constexpr const char* smoke_script =
        "local cube = nil\n"
        "function start()\n"
        "    cube = Engine.findEntity(\"BootstrapCube\")\n"
        "end\n"
        "function update(dt)\n"
        "    if cube ~= nil and cube:isAlive() then\n"
        "        cube:rotateY(dt * 1.5)\n"
        "        cube:move(dt, 0.0, 0.0)\n"
        "    end\n"
        "end\n";
    const gameengine::core::Status load_status =
        script_runtime.load_script(smoke_script, "lua_smoke_test.lua");
    if (!load_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(load_status.code));
        script_runtime.shutdown();
        return 1;
    }

    const gameengine::core::Status start_status = script_runtime.call_start(scene);
    const gameengine::core::Status update_status =
        start_status ? script_runtime.call_update(scene, 0.016F) : start_status;
    if (!update_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(update_status.code));
        script_runtime.shutdown();
        return 1;
    }

    const gameengine::scene::TransformComponent* updated = scene.transform(cube);
    if (updated == nullptr || updated->local_position.x <= 0.0F) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "lua smoke script did not update scene transform");
        script_runtime.shutdown();
        return 1;
    }

    const gameengine::scripting::ScriptMetrics metrics = script_runtime.metrics();
    std::fprintf(stdout,
                 "lua smoke metrics memory_bytes=%zu peak_bytes=%zu allocations=%llu updates=%llu\n",
                 metrics.memory_bytes,
                 metrics.peak_memory_bytes,
                 static_cast<unsigned long long>(metrics.allocation_count),
                 static_cast<unsigned long long>(metrics.update_calls));
    script_runtime.shutdown();
    return 0;
}

} // namespace
#endif

int main(int argc, char** argv)
{
    gameengine::core::Core core;

    const gameengine::core::Status core_status = core.initialize();
    if (!core_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(core_status.code));
        return 1;
    }

#if GAMEENGINE_PLATFORM_HAS_WINDOW
    bool smoke_test = false;
    bool metrics_test = false;
    bool renderer_benchmark = false;
    bool gpu_culling = false;
    bool audio_smoke_test = false;
    bool physics_demo = false;
    bool lua_smoke_test = false;
    bool lua_demo = false;
    std::filesystem::path scene_path{};
    std::filesystem::path assets_dir{};
    gameengine::renderer::quality::RendererQuality renderer_quality =
        gameengine::renderer::quality::RendererQuality::medium;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--smoke-test") == 0) {
            smoke_test = true;
        } else if (std::strcmp(argv[index], "--metrics") == 0) {
            metrics_test = true;
        } else if (std::strcmp(argv[index], "--renderer-benchmark") == 0) {
            renderer_benchmark = true;
        } else if (std::strcmp(argv[index], "--gpu-culling") == 0) {
            gpu_culling = true;
        } else if (std::strcmp(argv[index], "--audio-smoke-test") == 0) {
            audio_smoke_test = true;
        } else if (std::strcmp(argv[index], "--physics-demo") == 0) {
            physics_demo = true;
        } else if (std::strcmp(argv[index], "--lua-smoke-test") == 0) {
            lua_smoke_test = true;
        } else if (std::strcmp(argv[index], "--lua-demo") == 0) {
            lua_demo = true;
        } else if (std::strcmp(argv[index], "--scene") == 0) {
            if (index + 1 >= argc || argv[index + 1][0] == '\0') {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      "--scene expects a .gescene file path");
                core.shutdown();
                return 2;
            }
            scene_path = argv[++index];
        } else if (std::strcmp(argv[index], "--assets-dir") == 0) {
            if (index + 1 >= argc || argv[index + 1][0] == '\0') {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      "--assets-dir expects a directory path");
                core.shutdown();
                return 2;
            }
            assets_dir = argv[++index];
        } else if (std::strcmp(argv[index], "--renderer-quality") == 0) {
            if (index + 1 >= argc ||
                !gameengine::renderer::quality::parse(argv[++index], renderer_quality)) {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      "--renderer-quality expects low, medium or high");
                core.shutdown();
                return 2;
            }
        } else {
            gameengine::core::log(gameengine::core::LogLevel::error,
                              "unknown argument; use --smoke-test, --metrics, "
                                  "--renderer-benchmark, --gpu-culling, "
                                  "--audio-smoke-test, --physics-demo, "
                                  "--lua-smoke-test, --lua-demo, "
                                  "--scene <path>, --assets-dir <dir> or "
                                  "--renderer-quality low|medium|high");
            core.shutdown();
            return 2;
        }
    }
    if (!assets_dir.empty() && scene_path.empty()) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--assets-dir requires --scene");
        core.shutdown();
        return 2;
    }
    if ((smoke_test && metrics_test) || (smoke_test && renderer_benchmark) ||
        (metrics_test && renderer_benchmark)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--smoke-test, --metrics and --renderer-benchmark are exclusive");
        core.shutdown();
        return 2;
    }
    if (!scene_path.empty() && (metrics_test || renderer_benchmark)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--scene is exclusive with --metrics and --renderer-benchmark");
        core.shutdown();
        return 2;
    }
    if (audio_smoke_test &&
        (smoke_test || metrics_test || renderer_benchmark || gpu_culling ||
         physics_demo || lua_smoke_test || lua_demo || !scene_path.empty())) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--audio-smoke-test is exclusive with other modes");
        core.shutdown();
        return 2;
    }
    if (lua_smoke_test &&
        (smoke_test || metrics_test || renderer_benchmark || gpu_culling ||
         physics_demo || lua_demo || !scene_path.empty())) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--lua-smoke-test is exclusive with other modes");
        core.shutdown();
        return 2;
    }
    if (physics_demo && (metrics_test || renderer_benchmark || gpu_culling || lua_demo)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--physics-demo is exclusive with metrics, lua-demo and benchmark modes");
        core.shutdown();
        return 2;
    }
    if (lua_demo && (metrics_test || renderer_benchmark || gpu_culling)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--lua-demo is exclusive with metrics and benchmark modes");
        core.shutdown();
        return 2;
    }

#if !GAMEENGINE_BUILD_PHYSICS
    if (physics_demo) {
        gameengine::core::log(gameengine::core::LogLevel::info,
                              "physics: unavailable (physics module disabled)");
        if (smoke_test) {
            core.shutdown();
            return 0;
        }
    }
#endif

#if !GAMEENGINE_BUILD_LUA
    if (lua_demo) {
        gameengine::core::log(gameengine::core::LogLevel::info,
                              "lua: unavailable (lua module disabled)");
        if (smoke_test) {
            core.shutdown();
            return 0;
        }
    }
#endif

    if (audio_smoke_test) {
#if GAMEENGINE_BUILD_AUDIO
        const int audio_result = run_audio_smoke_test();
#else
        gameengine::core::log(gameengine::core::LogLevel::info,
                              "audio: unavailable (audio module disabled)");
        const int audio_result = 0;
#endif
        core.shutdown();
        return audio_result;
    }

    if (lua_smoke_test) {
#if GAMEENGINE_BUILD_LUA
        const int lua_result = run_lua_smoke_test();
#else
        gameengine::core::log(gameengine::core::LogLevel::info,
                              "lua: unavailable (lua module disabled)");
        const int lua_result = 0;
#endif
        core.shutdown();
        return lua_result;
    }

    gameengine::platform::Platform platform;
    const gameengine::core::Status platform_status = platform.initialize();
    if (!platform_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(platform_status.code));
        core.shutdown();
        return 3;
    }

    const gameengine::platform::WindowDescription window_description{};
    const gameengine::core::Status window_status = platform.create_window(window_description);
    if (!window_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(window_status.code));
        platform.shutdown();
        core.shutdown();
        return 4;
    }

#if GAMEENGINE_RENDERER_HAS_VULKAN
    gameengine::rhi::Renderer renderer;
    const gameengine::core::Status renderer_status = renderer.initialize(platform);
    if (!renderer_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(renderer_status.code));
        platform.shutdown();
        core.shutdown();
        return 5;
    }
    const gameengine::core::Status quality_status =
        gameengine::renderer::diagnostics::set_renderer_quality(renderer, renderer_quality);
    if (!quality_status) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              gameengine::core::to_string(quality_status.code));
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }
    if (gpu_culling) {
        const gameengine::core::Status visibility_status =
            gameengine::renderer::diagnostics::set_visibility_mode(
                renderer, gameengine::renderer::gpu_culling::VisibilityMode::gpu);
        if (!visibility_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(visibility_status.code));
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
    }
    gameengine::scene::Scene runtime_scene;
    gameengine::assets::PreparedAssetBundle prepared_assets;
#if GAMEENGINE_BUILD_ANIMATION
    gameengine::animation::AnimationSystem animation_system;
#endif
#if GAMEENGINE_BUILD_PHYSICS
    gameengine::physics::PhysicsWorld physics_world;
#endif
#if GAMEENGINE_BUILD_LUA
    gameengine::scripting::ScriptRuntime script_runtime;
#endif
    if (!scene_path.empty()) {
        const std::filesystem::path resolved_assets_dir =
            !assets_dir.empty()
                ? assets_dir
                : (scene_path.has_parent_path() ? scene_path.parent_path()
                                                : std::filesystem::path{"."});
        std::vector<std::byte> scene_bytes;
        gameengine::assets::SceneView scene_view{};
        const std::array<gameengine::core::u64, 1> builtin_meshes = {
            gameengine::scene::bootstrap_mesh_id,
        };
        const std::array<gameengine::core::u64, 1> builtin_materials = {
            gameengine::scene::bootstrap_material_id,
        };
        const auto asset_load_start = std::chrono::steady_clock::now();
        const gameengine::core::Status load_status = prepared_assets.load_directory(resolved_assets_dir);
        const auto asset_load_end = std::chrono::steady_clock::now();
        if (!load_status ||
            !gameengine::assets::read_binary_asset_file(scene_path, scene_bytes) ||
            !gameengine::assets::read_scene(scene_bytes, scene_view) ||
            !prepared_assets.validate_scene_dependencies(
                scene_view, builtin_meshes, builtin_materials) ||
            !gameengine::scene::deserialize_scene(scene_bytes, runtime_scene) ||
            !gameengine::scene::ensure_default_camera_and_light(runtime_scene) ||
            !gameengine::renderer::scene_bridge::set_prepared_assets(
                renderer,
                gameengine::renderer::scene_bridge::make_prepared_render_assets(prepared_assets))) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  "prepared scene or asset loading failed");
            static_cast<void>(gameengine::renderer::scene_bridge::clear_prepared_assets(renderer));
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
        const auto scene_load_end = std::chrono::steady_clock::now();
        const auto asset_load_ns = static_cast<unsigned long long>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(asset_load_end - asset_load_start)
                .count());
        const auto scene_load_ns = static_cast<unsigned long long>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(scene_load_end - asset_load_end)
                .count());
        std::fprintf(stderr,
                     "[gameengine] [info] scene_load_telemetry asset_load_ns=%llu "
                     "scene_load_ns=%llu assets=%zu entities=%zu\n",
                     asset_load_ns,
                     scene_load_ns,
                     prepared_assets.total_count(),
                     runtime_scene.entity_count());
    } else if (!gameengine::scene::create_bootstrap_scene(runtime_scene) ||
               runtime_scene.entities().empty()) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "runtime scene creation failed");
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }

    gameengine::scene::Entity primary_entity{};
    for (const gameengine::scene::Entity entity : runtime_scene.entities()) {
        if (runtime_scene.mesh_renderer(entity) != nullptr &&
            runtime_scene.transform(entity) != nullptr) {
            primary_entity = entity;
            break;
        }
    }
    if (!primary_entity.valid() && !runtime_scene.entities().empty()) {
        primary_entity = runtime_scene.entities().front();
    }
    const gameengine::scene::Entity bootstrap_entity = primary_entity;
    const gameengine::scene::TransformComponent* bootstrap_transform =
        runtime_scene.transform(bootstrap_entity);
    if (bootstrap_transform == nullptr) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "bootstrap transform missing");
        static_cast<void>(gameengine::renderer::scene_bridge::clear_prepared_assets(renderer));
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }

#if GAMEENGINE_BUILD_ANIMATION
    if (!physics_demo && !lua_demo) {
        if (!animation_system.reserve_players(1U) ||
            !animation_system.add_procedural_player(bootstrap_entity, *bootstrap_transform)) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  "animation player creation failed");
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
    }
#endif

#if GAMEENGINE_BUILD_PHYSICS
    if (physics_demo) {
        gameengine::scene::TransformComponent* cube_transform =
            runtime_scene.transform(bootstrap_entity);
        if (cube_transform != nullptr) {
            cube_transform->local_position = {0.0F, 4.0F, 0.0F};
            static_cast<void>(runtime_scene.update_transforms());
        }

        const gameengine::scene::Entity ground_entity = runtime_scene.create_entity();
        if (!ground_entity.valid() ||
            !runtime_scene.add_transform(ground_entity, {.local_position = {0.0F, -2.0F, 0.0F}}) ||
            !runtime_scene.update_transforms()) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  "physics ground entity creation failed");
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }

        const gameengine::core::Status physics_init_status = physics_world.initialize();
        if (!physics_init_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(physics_init_status.code));
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }

        const gameengine::core::Status ground_status = physics_world.add_body(
            runtime_scene, ground_entity,
            {.shape = gameengine::physics::ShapeType::box,
             .half_extents = {10.0F, 1.0F, 10.0F},
             .motion = gameengine::physics::MotionType::static_body,
             .friction = 0.6F});
        const gameengine::core::Status cube_status = physics_world.add_body(
            runtime_scene, bootstrap_entity,
            {.shape = gameengine::physics::ShapeType::box,
             .half_extents = {1.0F, 1.0F, 1.0F},
             .motion = gameengine::physics::MotionType::dynamic_body,
             .mass_kilograms = 5.0F,
             .friction = 0.6F,
             .restitution = 0.3F});
        if (!ground_status || !cube_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  "physics bodies creation failed");
            physics_world.shutdown();
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
    }
#endif

    if (!gameengine::renderer::scene_bridge::attach_scene(renderer, runtime_scene)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "runtime scene attachment failed");
#if GAMEENGINE_BUILD_PHYSICS
        physics_world.shutdown();
#endif
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }

#if GAMEENGINE_BUILD_LUA
    if (lua_demo) {
        const gameengine::core::Status lua_init_status = script_runtime.initialize();
        if (!lua_init_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(lua_init_status.code));
            static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_PHYSICS
            physics_world.shutdown();
#endif
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }

        const gameengine::core::Status lua_reg_status =
            script_runtime.register_entity("BootstrapCube", bootstrap_entity);
        constexpr const char* demo_script =
            "local cube = nil\n"
            "function start()\n"
            "    cube = Engine.findEntity(\"BootstrapCube\")\n"
            "end\n"
            "function update(dt)\n"
            "    if cube ~= nil and cube:isAlive() then\n"
            "        cube:rotateY(dt * 1.5)\n"
            "    end\n"
            "end\n";
        const gameengine::core::Status lua_load_status =
            lua_reg_status ? script_runtime.load_script(demo_script, "lua_demo.lua")
                           : lua_reg_status;
        const gameengine::core::Status lua_start_status =
            lua_load_status ? script_runtime.call_start(runtime_scene, &platform.input())
                            : lua_load_status;
        if (!lua_start_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(lua_start_status.code));
            script_runtime.shutdown();
            static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_PHYSICS
            physics_world.shutdown();
#endif
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
    }
#endif
    if (renderer_benchmark) {
        const gameengine::core::Status benchmark_status =
            gameengine::renderer::diagnostics::run_renderer_benchmark(renderer, gpu_culling);
        if (!benchmark_status &&
            benchmark_status.code != gameengine::core::ErrorCode::unsupported_platform) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(benchmark_status.code));
            static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
            static_cast<void>(gameengine::renderer::scene_bridge::clear_prepared_assets(renderer));
#if GAMEENGINE_BUILD_LUA
            script_runtime.shutdown();
#endif
#if GAMEENGINE_BUILD_PHYSICS
            physics_world.shutdown();
#endif
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
        static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
        static_cast<void>(gameengine::renderer::scene_bridge::clear_prepared_assets(renderer));
#if GAMEENGINE_BUILD_LUA
        script_runtime.shutdown();
#endif
#if GAMEENGINE_BUILD_PHYSICS
        physics_world.shutdown();
#endif
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 0;
    }
#endif

    gameengine::core::Clock clock;
    std::uint32_t metrics_frame_count = 0;
    std::size_t metrics_workload_index = 0;
    constexpr std::array<std::uint32_t, 3> metrics_workloads = {1'000U, 10'000U, 100'000U};
    constexpr std::uint32_t metrics_warmup_frames = 10;
    constexpr std::uint32_t metrics_measured_frames = 30;
    constexpr std::uint32_t metrics_frames_per_workload =
        metrics_warmup_frames + metrics_measured_frames;
#if !GAMEENGINE_RENDERER_HAS_VULKAN
    (void)metrics_frame_count;
    (void)metrics_test;
    (void)metrics_workload_index;
    (void)metrics_workloads;
    (void)metrics_warmup_frames;
    (void)metrics_measured_frames;
    (void)metrics_frames_per_workload;
    (void)renderer_quality;
    (void)scene_path;
    (void)assets_dir;

    if (renderer_benchmark) {
        gameengine::core::log(gameengine::core::LogLevel::info,
                              "renderer benchmark: unavailable (Vulkan disabled)");
        platform.shutdown();
        core.shutdown();
        return 0;
    }
#endif
#if GAMEENGINE_RENDERER_HAS_VULKAN
    if (metrics_test) {
        const gameengine::core::Status workload_status =
            gameengine::renderer::diagnostics::set_procedural_workload(
                renderer, metrics_workloads[metrics_workload_index]);
        if (!workload_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(workload_status.code));
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
    }
#endif
    do {
        platform.poll_events();
        const gameengine::core::f64 delta_seconds = clock.tick();
#if GAMEENGINE_RENDERER_HAS_VULKAN
#if GAMEENGINE_BUILD_PHYSICS
        if (physics_demo) {
            const gameengine::core::Status physics_status =
                physics_world.update(static_cast<gameengine::core::f32>(delta_seconds));
            if (!physics_status) {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      gameengine::core::to_string(physics_status.code));
                static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_LUA
                script_runtime.shutdown();
#endif
                physics_world.shutdown();
                renderer.shutdown();
                platform.shutdown();
                core.shutdown();
                return 6;
            }
            const gameengine::core::Status write_status =
                physics_world.write_transforms(runtime_scene);
            if (!write_status) {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      gameengine::core::to_string(write_status.code));
                static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_LUA
                script_runtime.shutdown();
#endif
                physics_world.shutdown();
                renderer.shutdown();
                platform.shutdown();
                core.shutdown();
                return 6;
            }
        }
#endif
#if GAMEENGINE_BUILD_LUA
        if (lua_demo) {
            const gameengine::core::Status lua_update_status = script_runtime.call_update(
                runtime_scene, static_cast<gameengine::core::f32>(delta_seconds), &platform.input());
            if (!lua_update_status) {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      gameengine::core::to_string(lua_update_status.code));
                static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
                script_runtime.shutdown();
#if GAMEENGINE_BUILD_PHYSICS
                physics_world.shutdown();
#endif
                renderer.shutdown();
                platform.shutdown();
                core.shutdown();
                return 6;
            }
        }
#endif
#if GAMEENGINE_BUILD_ANIMATION
        if (!physics_demo && !lua_demo) {
            const gameengine::core::Status animation_status = animation_system.update(
                runtime_scene, static_cast<gameengine::core::f32>(delta_seconds));
            if (!animation_status) {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      gameengine::core::to_string(animation_status.code));
                static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_LUA
                script_runtime.shutdown();
#endif
#if GAMEENGINE_BUILD_PHYSICS
                physics_world.shutdown();
#endif
                renderer.shutdown();
                platform.shutdown();
                core.shutdown();
                return 6;
            }
        }
#endif
#if !GAMEENGINE_BUILD_ANIMATION && !GAMEENGINE_BUILD_PHYSICS && !GAMEENGINE_BUILD_LUA
        (void)delta_seconds;
#endif
#else
        (void)delta_seconds;
#endif

#if GAMEENGINE_RENDERER_HAS_VULKAN
        if (metrics_test && metrics_frame_count == metrics_warmup_frames) {
            gameengine::renderer::diagnostics::begin_metrics(renderer);
        }
        const gameengine::core::Status frame_status = renderer.render_frame(platform);
        if (!frame_status) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(frame_status.code));
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
        if (metrics_test) {
            ++metrics_frame_count;
            if (metrics_frame_count >= metrics_frames_per_workload) {
                gameengine::renderer::diagnostics::print_metrics(renderer);
                ++metrics_workload_index;
                if (metrics_workload_index < metrics_workloads.size()) {
                    const gameengine::core::Status workload_status =
                        gameengine::renderer::diagnostics::set_procedural_workload(
                            renderer, metrics_workloads[metrics_workload_index]);
                    if (!workload_status) {
                        gameengine::core::log(gameengine::core::LogLevel::error,
                                              gameengine::core::to_string(workload_status.code));
                        renderer.shutdown();
                        platform.shutdown();
                        core.shutdown();
                        return 6;
                    }
                    metrics_frame_count = 0;
                } else {
                    platform.request_close();
                }
            }
        }
#endif

        if (smoke_test) {
            platform.request_close();
#if !GAMEENGINE_RENDERER_HAS_VULKAN
        } else if (metrics_test) {
            platform.request_close();
        } else if (renderer_benchmark) {
            platform.request_close();
#endif
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } while (!platform.should_close());

#if GAMEENGINE_RENDERER_HAS_VULKAN
    static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
    static_cast<void>(gameengine::renderer::scene_bridge::clear_prepared_assets(renderer));
#if GAMEENGINE_BUILD_LUA
    script_runtime.shutdown();
#endif
#if GAMEENGINE_BUILD_PHYSICS
    physics_world.shutdown();
#endif
    renderer.shutdown();
#endif
    platform.shutdown();
#else
    (void)argc;
    (void)argv;
#endif

    core.shutdown();
    return 0;
}
