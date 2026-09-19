#pragma once

#include <array>
#include <atomic>
#include <span>
#include <string_view>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"

#include "engine/audio/audio_backend.hpp"

namespace gameengine::audio {

inline constexpr core::u32 kSampleRate = 48'000U;
inline constexpr core::u16 kChannelCount = 2U;
inline constexpr core::u32 kFramesPerBuffer = 512U;
inline constexpr core::u32 kMaxVoices = 64U;
inline constexpr core::u32 kCommandQueueCapacity = 256U;

struct AudioConfiguration final {
    core::u32 sample_rate = kSampleRate;
    core::u16 channels = kChannelCount;
    core::u32 frames_per_buffer = kFramesPerBuffer;
    core::u32 max_voices = kMaxVoices;
    core::u32 command_queue_capacity = kCommandQueueCapacity;
    bool enable_output = true;
};

struct VoiceHandle final {
    core::u32 index = 0;
    core::u32 generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return generation != 0U && index < kMaxVoices;
    }

    [[nodiscard]] constexpr core::u64 packed() const noexcept
    {
        return (static_cast<core::u64>(generation) << 32U) | index;
    }

    friend constexpr bool operator==(VoiceHandle left, VoiceHandle right) noexcept = default;
};

struct ProceduralTone final {
    core::f32 frequency_hz = 440.0F;
    core::f32 duration_seconds = 0.25F;
    core::f32 gain = 0.15F;
    core::f32 pan = 0.0F;
    core::f32 pitch = 1.0F;
    bool loop = false;
};

struct AudioMetrics final {
    bool initialized = false;
    bool available = false;
    std::string_view backend{};
    core::u32 active_voices = 0;
    core::u64 underruns = 0;
    core::u64 frames_mixed = 0;
    core::u64 frames_played = 0;
    core::usize memory_reserved = 0;
};

class AudioSystem final {
public:
    AudioSystem() noexcept = default;
    ~AudioSystem() noexcept;

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    [[nodiscard]] core::Status initialize(
        const AudioConfiguration& configuration = {}) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] core::Status play_tone(const ProceduralTone& tone,
                                          VoiceHandle& handle) noexcept;
    [[nodiscard]] core::Status stop(VoiceHandle handle) noexcept;
    [[nodiscard]] core::Status stop_all() noexcept;
    [[nodiscard]] core::Status set_master_gain(core::f32 gain) noexcept;
    [[nodiscard]] core::Status update(core::f32 delta_seconds) noexcept;

    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool available() const noexcept { return backend_.available; }
    [[nodiscard]] std::string_view backend_name() const noexcept { return backend_.name; }
    [[nodiscard]] AudioMetrics metrics() const noexcept;
    [[nodiscard]] core::usize memory_usage_bytes() const noexcept;

    // Used by deterministic tests and the null backend. It must not be called
    // while a native backend is actively consuming the mixer.
    [[nodiscard]] core::Status render_for_testing(std::span<core::f32> interleaved_samples) noexcept;

private:
    enum class CommandType : core::u8 {
        play = 0,
        stop,
        stop_all,
        set_master_gain,
    };

    struct Command final {
        CommandType type = CommandType::stop_all;
        core::u32 index = 0;
        core::u32 generation = 0;
        ProceduralTone tone{};
        core::f32 value = 1.0F;
    };

    struct VoiceState final {
        bool active = false;
        core::u32 generation = 0;
        ProceduralTone tone{};
        core::f32 phase = 0.0F;
        core::f32 elapsed_seconds = 0.0F;
    };

    [[nodiscard]] core::Status validate_configuration(
        const AudioConfiguration& configuration) const noexcept;
    [[nodiscard]] core::Status validate_tone(const ProceduralTone& tone) const noexcept;
    [[nodiscard]] bool enqueue(const Command& command) noexcept;
    [[nodiscard]] bool reserve_voice(VoiceHandle& handle) noexcept;
    void release_reservation(core::u32 index) noexcept;
    void consume_commands() noexcept;
    void mix(core::f32* samples, core::u32 frames) noexcept;
    void mix_voice(core::u32 voice_index, core::f32* samples, core::u32 frames) noexcept;
    static void render_callback(void* user_data,
                                core::f32* samples,
                                core::u32 frames) noexcept;

    AudioConfiguration configuration_{};
    detail::AudioBackend backend_{};
    bool initialized_ = false;
    std::array<VoiceState, kMaxVoices> voices_{};
    std::array<std::atomic<core::u32>, kMaxVoices> generations_{};
    std::atomic<core::u64> reserved_mask_{0};
    std::atomic<core::u32> active_voices_{0};
    std::array<Command, kCommandQueueCapacity> command_queue_{};
    std::atomic<core::u32> command_head_{0};
    std::atomic<core::u32> command_tail_{0};
    core::f32 master_gain_ = 1.0F;
    std::atomic<core::u64> underruns_{0};
    std::atomic<core::u64> frames_mixed_{0};
    std::atomic<core::u64> frames_played_{0};
};

} // namespace gameengine::audio
