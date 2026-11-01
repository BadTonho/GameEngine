#include "engine/core/core.hpp"
#include "engine/core/clock.hpp"
#include "engine/core/diagnostics.hpp"
#if GAMEENGINE_BUILD_AUDIO
#include "engine/audio/audio.hpp"
#endif
#include "engine/platform/platform.hpp"
#if GAMEENGINE_BUILD_ANIMATION || GAMEENGINE_BUILD_PHYSICS
#include "engine/renderer/scene_bridge.hpp"
#include "engine/scene/scene.hpp"
#endif
#if GAMEENGINE_BUILD_ANIMATION
#include "engine/animation/animation.hpp"
#endif
#if GAMEENGINE_BUILD_PHYSICS
#include "engine/physics/physics.hpp"
#endif
#include "engine/renderer/renderer_metrics.hpp"
#include "engine/renderer/renderer_quality.hpp"
#include "engine/rhi/rhi.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <thread>

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
                                  "--audio-smoke-test, --physics-demo or "
                                  "--renderer-quality low|medium|high");
            core.shutdown();
            return 2;
        }
    }
    if ((smoke_test && metrics_test) || (smoke_test && renderer_benchmark) ||
        (metrics_test && renderer_benchmark)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--smoke-test, --metrics and --renderer-benchmark are exclusive");
        core.shutdown();
        return 2;
    }
    if (audio_smoke_test &&
        (smoke_test || metrics_test || renderer_benchmark || gpu_culling)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--audio-smoke-test is exclusive with renderer modes");
        core.shutdown();
        return 2;
    }
    if (physics_demo && (metrics_test || renderer_benchmark || gpu_culling || audio_smoke_test)) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "--physics-demo is exclusive with metrics, audio and benchmark modes");
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
#if (GAMEENGINE_BUILD_ANIMATION || GAMEENGINE_BUILD_PHYSICS)
    gameengine::scene::Scene runtime_scene;
#if GAMEENGINE_BUILD_ANIMATION
    gameengine::animation::AnimationSystem animation_system;
#endif
#if GAMEENGINE_BUILD_PHYSICS
    gameengine::physics::PhysicsWorld physics_world;
#endif
    if (!gameengine::scene::create_bootstrap_scene(runtime_scene) ||
        runtime_scene.entities().empty()) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "runtime scene creation failed");
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }
    const gameengine::scene::Entity bootstrap_entity = runtime_scene.entities().front();
    const gameengine::scene::TransformComponent* bootstrap_transform =
        runtime_scene.transform(bootstrap_entity);
    if (bootstrap_transform == nullptr) {
        gameengine::core::log(gameengine::core::LogLevel::error,
                              "bootstrap transform missing");
        renderer.shutdown();
        platform.shutdown();
        core.shutdown();
        return 6;
    }

#if GAMEENGINE_BUILD_ANIMATION
    if (!physics_demo) {
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
#endif
    if (renderer_benchmark) {
        const gameengine::core::Status benchmark_status =
            gameengine::renderer::diagnostics::run_renderer_benchmark(renderer, gpu_culling);
        if (!benchmark_status &&
            benchmark_status.code != gameengine::core::ErrorCode::unsupported_platform) {
            gameengine::core::log(gameengine::core::LogLevel::error,
                                  gameengine::core::to_string(benchmark_status.code));
#if GAMEENGINE_BUILD_ANIMATION || GAMEENGINE_BUILD_PHYSICS
            static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_PHYSICS
            physics_world.shutdown();
#endif
#endif
            renderer.shutdown();
            platform.shutdown();
            core.shutdown();
            return 6;
        }
#if GAMEENGINE_BUILD_ANIMATION || GAMEENGINE_BUILD_PHYSICS
        static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_PHYSICS
        physics_world.shutdown();
#endif
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
                physics_world.shutdown();
                renderer.shutdown();
                platform.shutdown();
                core.shutdown();
                return 6;
            }
        }
#endif
#if GAMEENGINE_BUILD_ANIMATION
        if (!physics_demo) {
            const gameengine::core::Status animation_status = animation_system.update(
                runtime_scene, static_cast<gameengine::core::f32>(delta_seconds));
            if (!animation_status) {
                gameengine::core::log(gameengine::core::LogLevel::error,
                                      gameengine::core::to_string(animation_status.code));
                static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
                renderer.shutdown();
                platform.shutdown();
                core.shutdown();
                return 6;
            }
        }
#endif
#if !GAMEENGINE_BUILD_ANIMATION && !GAMEENGINE_BUILD_PHYSICS
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
#if GAMEENGINE_BUILD_ANIMATION || GAMEENGINE_BUILD_PHYSICS
    static_cast<void>(gameengine::renderer::scene_bridge::detach_scene(renderer));
#if GAMEENGINE_BUILD_PHYSICS
    physics_world.shutdown();
#endif
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
