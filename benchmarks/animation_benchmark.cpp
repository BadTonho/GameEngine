#include "engine/animation/animation.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

namespace {

using namespace gameengine;

void run_case(core::u32 count)
{
    scene::Scene scene;
    scene.reserve(count);
    std::vector<scene::Entity> entities;
    entities.reserve(count);
    for (core::u32 index = 0; index < count; ++index) {
        const scene::Entity entity = scene.create_entity();
        entities.push_back(entity);
        static_cast<void>(scene.add_transform(entity));
    }

    std::vector<animation::TransformKeyframe> keyframes;
    keyframes.reserve(static_cast<std::size_t>(count) * 2U);
    std::vector<animation::TransformTrack> tracks;
    tracks.reserve(count);
    for (const scene::Entity entity : entities) {
        keyframes.push_back({0.0F, {}, math::Quaternion::identity(), {1.0F, 1.0F, 1.0F}});
        keyframes.push_back({1.0F, {1.0F, 0.0F, 0.0F}, math::Quaternion::identity(),
                             {1.0F, 1.0F, 1.0F}});
        tracks.push_back({entity, std::span<const animation::TransformKeyframe>{
                                   keyframes.data() + keyframes.size() - 2U, 2U}});
    }
    const animation::AnimationClip clip{
        "benchmark", 1.0F, animation::PlaybackMode::loop,
        std::span<const animation::TransformTrack>{tracks}};
    animation::AnimationSystem system;
    static_cast<void>(system.reserve_players(count));
    for (core::u32 index = 0; index < count; ++index) {
        static_cast<void>(system.add_player(clip, index));
    }

    const auto start = std::chrono::steady_clock::now();
    static_cast<void>(system.update(scene, 1.0F / 60.0F));
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("animation count=%u update_ns=%lld scene_bytes=%zu system_bytes=%zu\n",
                count,
                static_cast<long long>(elapsed),
                scene.memory_usage_bytes(),
                system.memory_usage_bytes());
}

} // namespace

int main()
{
    run_case(1'000U);
    run_case(10'000U);
    run_case(100'000U);
    return 0;
}
