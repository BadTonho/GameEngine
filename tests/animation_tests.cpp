#include "engine/animation/animation.hpp"

#include <array>
#include <cmath>
#include <cstdio>

namespace {

using namespace gameengine;

bool expect(bool condition)
{
    if (!condition) {
        std::fprintf(stderr, "animation test failure\n");
    }
    return condition;
}

bool near(core::f32 left, core::f32 right, core::f32 epsilon = 0.0005F)
{
    return std::fabs(left - right) <= epsilon;
}

bool finite_transform(const animation::TransformSample& sample)
{
    return math::is_finite(sample.position) && math::is_finite(sample.scale) &&
           std::isfinite(sample.rotation.x) && std::isfinite(sample.rotation.y) &&
           std::isfinite(sample.rotation.z) && std::isfinite(sample.rotation.w);
}

} // namespace

int main()
{
    bool passed = true;
    const scene::Entity entity = scene::Entity::make(7U, 2U);
    const std::array<animation::TransformKeyframe, 2> keyframes = {
        animation::TransformKeyframe{0.0F,
                                     {0.0F, 0.0F, 0.0F},
                                     math::Quaternion::identity(),
                                     {1.0F, 1.0F, 1.0F}},
        animation::TransformKeyframe{1.0F,
                                     {2.0F, 4.0F, 6.0F},
                                     math::quaternion_from_axis_angle({0.0F, 1.0F, 0.0F},
                                                                        3.14159265F),
                                     {2.0F, 2.0F, 2.0F}},
    };
    const std::array<animation::TransformTrack, 1> tracks = {
        animation::TransformTrack{entity, std::span<const animation::TransformKeyframe>{keyframes}},
    };
    const animation::AnimationClip clip{
        "test_clip", 1.0F, animation::PlaybackMode::loop,
        std::span<const animation::TransformTrack>{tracks}};

    passed = expect(animation::validate_clip(clip).ok()) && passed;
    animation::TransformSample sample{};
    passed = expect(animation::sample_track(tracks[0], 0.5F, sample).ok()) && passed;
    passed = expect(near(sample.position.x, 1.0F) && near(sample.position.y, 2.0F) &&
                    near(sample.position.z, 3.0F) && near(sample.scale.x, 1.5F) &&
                    finite_transform(sample)) &&
             passed;

    const std::array<animation::TransformKeyframe, 2> duplicate_times = {
        keyframes[0],
        animation::TransformKeyframe{0.0F, keyframes[1].position, keyframes[1].rotation,
                                     keyframes[1].scale},
    };
    const std::array<animation::TransformTrack, 1> invalid_tracks = {
        animation::TransformTrack{entity,
                                  std::span<const animation::TransformKeyframe>{duplicate_times}},
    };
    passed = expect(!animation::validate_clip(
                          {"invalid", 1.0F, animation::PlaybackMode::loop,
                           std::span<const animation::TransformTrack>{invalid_tracks}})) &&
             passed;

    scene::Scene scene;
    const scene::Entity scene_entity = scene.create_entity();
    passed = expect(scene_entity.valid() && scene.add_transform(scene_entity).ok()) && passed;
    const std::array<animation::TransformKeyframe, 2> scene_keyframes = {
        animation::TransformKeyframe{0.0F, {0.0F, 0.0F, 0.0F}, math::Quaternion::identity(),
                                     {1.0F, 1.0F, 1.0F}},
        animation::TransformKeyframe{1.0F, {1.0F, 0.0F, 0.0F}, math::Quaternion::identity(),
                                     {1.0F, 1.0F, 1.0F}},
    };
    const std::array<animation::TransformTrack, 1> scene_tracks = {
        animation::TransformTrack{scene_entity,
                                  std::span<const animation::TransformKeyframe>{scene_keyframes}},
    };
    const animation::AnimationClip scene_clip{
        "scene_clip", 1.0F, animation::PlaybackMode::loop,
        std::span<const animation::TransformTrack>{scene_tracks}};
    animation::AnimationSystem system;
    passed = expect(system.reserve_players(1U).ok() && system.add_player(scene_clip).ok()) && passed;
    const core::usize memory_before = system.memory_usage_bytes();
    passed = expect(system.update(scene, 0.5F).ok()) && passed;
    const scene::TransformComponent* transform = scene.transform(scene_entity);
    passed = expect(transform != nullptr && near(transform->local_position.x, 0.5F) &&
                    system.memory_usage_bytes() == memory_before) &&
             passed;
    passed = expect(system.update(scene, 0.6F).ok() &&
                    near(scene.transform(scene_entity)->local_position.x, 0.1F)) &&
             passed;

    animation::AnimationSystem fixed_system;
    passed = expect(fixed_system.reserve_players(1U).ok() &&
                    fixed_system.add_player(scene_clip).ok()) &&
             passed;
    passed = expect(fixed_system.update(scene, 0.5F,
                                        {.fixed_step = true,
                                         .fixed_step_seconds = 0.25F,
                                         .max_steps = 4U})
                        .ok()) &&
             passed;
    passed = expect(near(scene.transform(scene_entity)->local_position.x, 0.5F)) && passed;

    scene::Scene stale_scene;
    const scene::Entity stale = stale_scene.create_entity();
    passed = expect(stale_scene.add_transform(stale).ok() && stale_scene.destroy_entity(stale).ok()) &&
             passed;
    const std::array<animation::TransformTrack, 1> stale_tracks = {
        animation::TransformTrack{stale,
                                  std::span<const animation::TransformKeyframe>{scene_keyframes}},
    };
    animation::AnimationSystem stale_system;
    passed = expect(stale_system.add_player(
                          {"stale", 1.0F, animation::PlaybackMode::loop,
                           std::span<const animation::TransformTrack>{stale_tracks}})
                        .ok()) &&
             passed;
    passed = expect(stale_system.update(stale_scene, 0.1F).code ==
                    core::ErrorCode::invalid_argument) &&
             passed;

    return passed ? 0 : 1;
}
