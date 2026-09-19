#include "engine/animation/animation.hpp"

#include <algorithm>
#include <cmath>

namespace gameengine::animation {

namespace {

constexpr core::f32 validation_epsilon = 0.00001F;

[[nodiscard]] core::Status invalid() noexcept
{
    return core::Status{core::ErrorCode::invalid_argument};
}

[[nodiscard]] bool finite(const math::Quaternion& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
           std::isfinite(value.w);
}

[[nodiscard]] bool valid_sample(const TransformSample& sample) noexcept
{
    const math::Quaternion& rotation = sample.rotation;
    const core::f32 rotation_length = rotation.x * rotation.x + rotation.y * rotation.y +
                                      rotation.z * rotation.z + rotation.w * rotation.w;
    return math::is_finite(sample.position) && math::is_finite(sample.scale) &&
           finite(rotation) && std::isfinite(rotation_length) && rotation_length > 0.0F;
}

[[nodiscard]] math::Quaternion nlerp(const math::Quaternion& left,
                                     const math::Quaternion& right,
                                     core::f32 amount) noexcept
{
    math::Quaternion adjusted_right = right;
    const core::f32 dot = left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w;
    if (dot < 0.0F) {
        adjusted_right = {-right.x, -right.y, -right.z, -right.w};
    }
    const math::Quaternion interpolated{
        left.x + (adjusted_right.x - left.x) * amount,
        left.y + (adjusted_right.y - left.y) * amount,
        left.z + (adjusted_right.z - left.z) * amount,
        left.w + (adjusted_right.w - left.w) * amount,
    };
    return math::normalize(interpolated);
}

[[nodiscard]] core::Status validate_track_for_clip(const AnimationClip& clip,
                                                   const TransformTrack& track) noexcept
{
    if (!track.target.valid() || track.keyframes.size() < 2U) {
        return invalid();
    }
    const TransformKeyframe& first = track.keyframes.front();
    const TransformKeyframe& last = track.keyframes.back();
    if (std::fabs(first.time_seconds) > validation_epsilon ||
        std::fabs(last.time_seconds - clip.duration_seconds) > validation_epsilon) {
        return invalid();
    }
    core::f32 previous_time = -1.0F;
    for (const TransformKeyframe& keyframe : track.keyframes) {
        if (!std::isfinite(keyframe.time_seconds) || keyframe.time_seconds < 0.0F ||
            keyframe.time_seconds <= previous_time ||
            keyframe.time_seconds > clip.duration_seconds + validation_epsilon) {
            return invalid();
        }
        const TransformSample sample{keyframe.position, keyframe.rotation, keyframe.scale};
        if (!valid_sample(sample)) {
            return invalid();
        }
        previous_time = keyframe.time_seconds;
    }
    return core::Status{};
}

void advance_player(AnimationPlayer& player, core::f32 delta_seconds) noexcept
{
    const AnimationClip& clip = player.clip;
    const core::f32 next_time = player.time_seconds + delta_seconds;
    if (clip.playback == PlaybackMode::loop) {
        player.time_seconds = std::fmod(next_time, clip.duration_seconds);
        if (player.time_seconds < 0.0F) {
            player.time_seconds += clip.duration_seconds;
        }
        return;
    }
    player.time_seconds = std::min(next_time, clip.duration_seconds);
    if (player.time_seconds >= clip.duration_seconds - validation_epsilon) {
        player.time_seconds = clip.duration_seconds;
        player.playing = false;
    }
}

} // namespace

core::Status validate_clip(const AnimationClip& clip) noexcept
{
    if (clip.name.empty() || !std::isfinite(clip.duration_seconds) ||
        clip.duration_seconds <= validation_epsilon || clip.tracks.empty() ||
        (clip.playback != PlaybackMode::once && clip.playback != PlaybackMode::loop)) {
        return invalid();
    }
    for (const TransformTrack& track : clip.tracks) {
        if (!validate_track_for_clip(clip, track)) {
            return invalid();
        }
    }
    return core::Status{};
}

core::Status sample_track(const TransformTrack& track,
                          core::f32 time_seconds,
                          TransformSample& sample) noexcept
{
    if (track.keyframes.empty() || !std::isfinite(time_seconds)) {
        return invalid();
    }
    const TransformKeyframe& first = track.keyframes.front();
    const TransformKeyframe& last = track.keyframes.back();
    const core::f32 clamped_time = std::clamp(time_seconds, first.time_seconds, last.time_seconds);
    if (clamped_time <= first.time_seconds) {
        sample = {first.position, math::normalize(first.rotation), first.scale};
        return core::Status{};
    }
    if (clamped_time >= last.time_seconds) {
        sample = {last.position, math::normalize(last.rotation), last.scale};
        return core::Status{};
    }

    for (core::usize index = 1; index < track.keyframes.size(); ++index) {
        const TransformKeyframe& right = track.keyframes[index];
        if (clamped_time > right.time_seconds) {
            continue;
        }
        const TransformKeyframe& left = track.keyframes[index - 1U];
        const core::f32 interval = right.time_seconds - left.time_seconds;
        const core::f32 amount = interval <= 0.0F
            ? 0.0F
            : (clamped_time - left.time_seconds) / interval;
        sample.position = {
            left.position.x + (right.position.x - left.position.x) * amount,
            left.position.y + (right.position.y - left.position.y) * amount,
            left.position.z + (right.position.z - left.position.z) * amount,
        };
        sample.scale = {
            left.scale.x + (right.scale.x - left.scale.x) * amount,
            left.scale.y + (right.scale.y - left.scale.y) * amount,
            left.scale.z + (right.scale.z - left.scale.z) * amount,
        };
        sample.rotation = nlerp(math::normalize(left.rotation),
                                math::normalize(right.rotation),
                                amount);
        return core::Status{};
    }
    return invalid();
}

core::Status AnimationSystem::reserve_players(core::usize count) noexcept
{
    players_.reserve(count);
    return core::Status{};
}

core::Status AnimationSystem::add_player(const AnimationClip& clip, core::u32 track_index) noexcept
{
    if (clip.name.empty() || !std::isfinite(clip.duration_seconds) ||
        clip.duration_seconds <= validation_epsilon || clip.tracks.empty() ||
        (clip.playback != PlaybackMode::once && clip.playback != PlaybackMode::loop) ||
        track_index >= clip.tracks.size() ||
        !validate_track_for_clip(clip, clip.tracks[track_index])) {
        return invalid();
    }
    const scene::Entity target = clip.tracks[track_index].target;
    if (find_player(target) != nullptr) {
        return invalid();
    }
    AnimationPlayer player{};
    player.clip = clip;
    player.track_index = track_index;
    const auto position = std::lower_bound(
        players_.begin(), players_.end(), target, [](const AnimationPlayer& candidate,
                                                     scene::Entity entity) noexcept {
            return candidate.clip.tracks[candidate.track_index].target.index() < entity.index();
        });
    players_.insert(position, player);
    return core::Status{};
}

core::Status AnimationSystem::configure_procedural_clip(
    scene::Entity entity,
    const scene::TransformComponent& initial_transform) noexcept
{
    const math::Quaternion half_turn =
        math::quaternion_from_axis_angle({0.0F, 1.0F, 0.0F}, 3.14159265F);
    const math::Quaternion full_turn =
        math::quaternion_from_axis_angle({0.0F, 1.0F, 0.0F}, 6.28318531F);
    const math::Quaternion base_rotation = math::normalize(initial_transform.local_rotation);
    math::Vec3 lifted_position = initial_transform.local_position;
    lifted_position.y += 0.15F;
    const math::Vec3 enlarged_scale = math::multiply(initial_transform.local_scale, 1.1F);
    procedural_keyframes_ = {
        TransformKeyframe{0.0F, initial_transform.local_position, base_rotation,
                          initial_transform.local_scale},
        TransformKeyframe{1.0F, lifted_position,
                          math::normalize(math::multiply(base_rotation, half_turn)), enlarged_scale},
        TransformKeyframe{2.0F, initial_transform.local_position,
                          math::normalize(math::multiply(base_rotation, full_turn)),
                          initial_transform.local_scale},
    };
    procedural_tracks_[0] = {entity, std::span<const TransformKeyframe>{procedural_keyframes_}};
    procedural_clip_ = {"procedural_cube", 2.0F, PlaybackMode::loop,
                        std::span<const TransformTrack>{procedural_tracks_}};
    return core::Status{};
}

core::Status AnimationSystem::add_procedural_player(
    scene::Entity entity,
    const scene::TransformComponent& initial_transform) noexcept
{
    if (!entity.valid() || find_player(entity) != nullptr ||
        !math::is_finite(initial_transform.local_position) ||
        !math::is_finite(initial_transform.local_scale)) {
        return invalid();
    }
    if (!configure_procedural_clip(entity, initial_transform)) {
        return invalid();
    }
    return add_player(procedural_clip_);
}

core::Status AnimationSystem::remove_player(scene::Entity entity) noexcept
{
    const auto iterator = std::lower_bound(
        players_.begin(), players_.end(), entity.index(), [](const AnimationPlayer& candidate,
                                                              core::u32 index) noexcept {
            return candidate.clip.tracks[candidate.track_index].target.index() < index;
        });
    if (iterator == players_.end() ||
        iterator->clip.tracks[iterator->track_index].target != entity) {
        return invalid();
    }
    players_.erase(iterator);
    return core::Status{};
}

AnimationPlayer* AnimationSystem::find_player(scene::Entity entity) noexcept
{
    const auto iterator = std::lower_bound(
        players_.begin(), players_.end(), entity.index(), [](const AnimationPlayer& candidate,
                                                              core::u32 index) noexcept {
            return candidate.clip.tracks[candidate.track_index].target.index() < index;
        });
    if (iterator != players_.end() &&
        iterator->clip.tracks[iterator->track_index].target == entity) {
        return &*iterator;
    }
    return nullptr;
}

const AnimationPlayer* AnimationSystem::find_player(scene::Entity entity) const noexcept
{
    const auto iterator = std::lower_bound(
        players_.begin(), players_.end(), entity.index(), [](const AnimationPlayer& candidate,
                                                              core::u32 index) noexcept {
            return candidate.clip.tracks[candidate.track_index].target.index() < index;
        });
    if (iterator != players_.end() &&
        iterator->clip.tracks[iterator->track_index].target == entity) {
        return &*iterator;
    }
    return nullptr;
}

core::Status AnimationSystem::pause(scene::Entity entity) noexcept
{
    AnimationPlayer* player_value = find_player(entity);
    if (player_value == nullptr) {
        return invalid();
    }
    player_value->playing = false;
    return core::Status{};
}

core::Status AnimationSystem::play(scene::Entity entity) noexcept
{
    AnimationPlayer* player_value = find_player(entity);
    if (player_value == nullptr) {
        return invalid();
    }
    player_value->playing = true;
    return core::Status{};
}

void AnimationSystem::pause_all() noexcept
{
    for (AnimationPlayer& player : players_) {
        player.playing = false;
    }
}

void AnimationSystem::play_all() noexcept
{
    for (AnimationPlayer& player : players_) {
        player.playing = true;
    }
}

core::Status AnimationSystem::update(scene::Scene& scene,
                                     core::f32 delta_seconds,
                                     const AnimationUpdateOptions& options) noexcept
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F ||
        (options.fixed_step &&
         (!std::isfinite(options.fixed_step_seconds) || options.fixed_step_seconds <= 0.0F ||
          options.max_steps == 0U))) {
        return invalid();
    }
    for (const AnimationPlayer& player : players_) {
        if (player.track_index >= player.clip.tracks.size()) {
            return invalid();
        }
        const scene::Entity entity = player.clip.tracks[player.track_index].target;
        if (!scene.is_alive(entity) || scene.transform(entity) == nullptr) {
            return invalid();
        }
    }

    bool changed = false;
    for (AnimationPlayer& player : players_) {
        if (!player.playing) {
            continue;
        }
        core::u32 steps = 0U;
        if (options.fixed_step) {
            player.fixed_accumulator += delta_seconds;
            while (player.fixed_accumulator + validation_epsilon >= options.fixed_step_seconds &&
                   steps < options.max_steps && player.playing) {
                advance_player(player, options.fixed_step_seconds);
                player.fixed_accumulator -= options.fixed_step_seconds;
                ++steps;
            }
            if (steps == options.max_steps &&
                player.fixed_accumulator >= options.fixed_step_seconds) {
                player.fixed_accumulator = std::fmod(player.fixed_accumulator,
                                                     options.fixed_step_seconds);
            }
        } else if (delta_seconds > 0.0F) {
            advance_player(player, delta_seconds);
            steps = 1U;
        }
        if (steps == 0U && player.initialized) {
            continue;
        }
        TransformSample sample{};
        const TransformTrack& track = player.clip.tracks[player.track_index];
        if (!sample_track(track, player.time_seconds, sample)) {
            return invalid();
        }
        scene::TransformComponent* transform = scene.transform(track.target);
        if (transform == nullptr) {
            return invalid();
        }
        transform->local_position = sample.position;
        transform->local_rotation = sample.rotation;
        transform->local_scale = sample.scale;
        player.initialized = true;
        changed = true;
    }
    return changed ? scene.update_transforms() : core::Status{};
}

const AnimationPlayer* AnimationSystem::player(scene::Entity entity) const noexcept
{
    return find_player(entity);
}

core::usize AnimationSystem::memory_usage_bytes() const noexcept
{
    return players_.capacity() * sizeof(AnimationPlayer) + sizeof(procedural_keyframes_) +
           sizeof(procedural_tracks_) + sizeof(procedural_clip_);
}

} // namespace gameengine::animation
