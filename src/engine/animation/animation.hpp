#pragma once

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/math/math.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::animation {

enum class PlaybackMode : core::u8 {
    once = 0,
    loop,
};

struct TransformKeyframe final {
    core::f32 time_seconds = 0.0F;
    math::Vec3 position{};
    math::Quaternion rotation = math::Quaternion::identity();
    math::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct TransformTrack final {
    scene::Entity target{};
    std::span<const TransformKeyframe> keyframes{};
};

struct AnimationClip final {
    std::string_view name{};
    core::f32 duration_seconds = 0.0F;
    PlaybackMode playback = PlaybackMode::loop;
    std::span<const TransformTrack> tracks{};
};

struct TransformSample final {
    math::Vec3 position{};
    math::Quaternion rotation = math::Quaternion::identity();
    math::Vec3 scale{1.0F, 1.0F, 1.0F};
};

struct AnimationUpdateOptions final {
    bool fixed_step = false;
    core::f32 fixed_step_seconds = 1.0F / 60.0F;
    core::u32 max_steps = 8U;
};

struct AnimationPlayer final {
    AnimationClip clip{};
    core::u32 track_index = 0;
    core::f32 time_seconds = 0.0F;
    core::f32 fixed_accumulator = 0.0F;
    bool playing = true;
    bool initialized = false;
};

[[nodiscard]] core::Status validate_clip(const AnimationClip& clip) noexcept;
[[nodiscard]] core::Status sample_track(const TransformTrack& track,
                                         core::f32 time_seconds,
                                         TransformSample& sample) noexcept;

class AnimationSystem final {
public:
    AnimationSystem() noexcept = default;
    ~AnimationSystem() noexcept = default;

    AnimationSystem(const AnimationSystem&) = delete;
    AnimationSystem& operator=(const AnimationSystem&) = delete;

    [[nodiscard]] core::Status reserve_players(core::usize count) noexcept;
    [[nodiscard]] core::Status add_player(const AnimationClip& clip,
                                           core::u32 track_index = 0U) noexcept;
    [[nodiscard]] core::Status add_procedural_player(
        scene::Entity entity,
        const scene::TransformComponent& initial_transform) noexcept;
    [[nodiscard]] core::Status remove_player(scene::Entity entity) noexcept;
    [[nodiscard]] core::Status pause(scene::Entity entity) noexcept;
    [[nodiscard]] core::Status play(scene::Entity entity) noexcept;
    void pause_all() noexcept;
    void play_all() noexcept;

    [[nodiscard]] core::Status update(
        scene::Scene& scene,
        core::f32 delta_seconds,
        const AnimationUpdateOptions& options = {}) noexcept;

    [[nodiscard]] const AnimationPlayer* player(scene::Entity entity) const noexcept;
    [[nodiscard]] std::span<const AnimationPlayer> players() const noexcept { return players_; }
    [[nodiscard]] core::usize memory_usage_bytes() const noexcept;

private:
    [[nodiscard]] core::Status configure_procedural_clip(
        scene::Entity entity,
        const scene::TransformComponent& initial_transform) noexcept;
    [[nodiscard]] AnimationPlayer* find_player(scene::Entity entity) noexcept;
    [[nodiscard]] const AnimationPlayer* find_player(scene::Entity entity) const noexcept;

    std::vector<AnimationPlayer> players_;
    std::array<TransformKeyframe, 3> procedural_keyframes_{};
    std::array<TransformTrack, 1> procedural_tracks_{};
    AnimationClip procedural_clip_{};
};

} // namespace gameengine::animation
