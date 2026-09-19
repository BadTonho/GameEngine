#include "engine/audio/audio.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gameengine::audio {

namespace {

constexpr core::f32 pi = 3.14159265358979323846F;
constexpr core::f32 two_pi = 2.0F * pi;

[[nodiscard]] core::Status invalid() noexcept
{
    return core::Status{core::ErrorCode::invalid_argument};
}

[[nodiscard]] bool finite(core::f32 value) noexcept
{
    return std::isfinite(value) != 0;
}

} // namespace

AudioSystem::~AudioSystem() noexcept
{
    shutdown();
}

core::Status AudioSystem::validate_configuration(
    const AudioConfiguration& configuration) const noexcept
{
    if (configuration.sample_rate != kSampleRate || configuration.channels != kChannelCount ||
        configuration.frames_per_buffer != kFramesPerBuffer ||
        configuration.max_voices != kMaxVoices ||
        configuration.command_queue_capacity != kCommandQueueCapacity) {
        return invalid();
    }
    return core::Status{};
}

core::Status AudioSystem::validate_tone(const ProceduralTone& tone) const noexcept
{
    if (!finite(tone.frequency_hz) || tone.frequency_hz < 20.0F ||
        tone.frequency_hz > 20'000.0F || !finite(tone.duration_seconds) ||
        tone.duration_seconds <= 0.0F || tone.duration_seconds > 3'600.0F ||
        !finite(tone.gain) || tone.gain < 0.0F || tone.gain > 1.0F || !finite(tone.pan) ||
        tone.pan < -1.0F || tone.pan > 1.0F || !finite(tone.pitch) || tone.pitch < 0.25F ||
        tone.pitch > 4.0F) {
        return invalid();
    }
    return core::Status{};
}

core::Status AudioSystem::initialize(const AudioConfiguration& configuration) noexcept
{
    if (initialized_) {
        return core::Status{core::ErrorCode::already_initialized};
    }
    if (!validate_configuration(configuration)) {
        return invalid();
    }

    configuration_ = configuration;
    command_head_.store(0U, std::memory_order_relaxed);
    command_tail_.store(0U, std::memory_order_relaxed);
    reserved_mask_.store(0U, std::memory_order_relaxed);
    active_voices_.store(0U, std::memory_order_relaxed);
    underruns_.store(0U, std::memory_order_relaxed);
    frames_mixed_.store(0U, std::memory_order_relaxed);
    frames_played_.store(0U, std::memory_order_relaxed);
    master_gain_ = 1.0F;
    for (core::u32 index = 0U; index < kMaxVoices; ++index) {
        voices_[index] = {};
        generations_[index].store(0U, std::memory_order_relaxed);
    }

    if (!configuration_.enable_output) {
        backend_ = {};
        backend_.name = "stub";
        initialized_ = true;
        return core::Status{};
    }

    const detail::BackendCallbacks callbacks{
        .user_data = this,
        .render = &AudioSystem::render_callback,
        .underruns = &underruns_,
        .frames_played = &frames_played_,
    };
    const core::Status backend_status = detail::initialize_backend(backend_, configuration_, callbacks);
    if (!backend_status) {
        backend_ = {};
        return backend_status;
    }
    initialized_ = true;
    return core::Status{};
}

void AudioSystem::shutdown() noexcept
{
    if (!initialized_ && backend_.state == nullptr) {
        return;
    }
    detail::shutdown_backend(backend_);
    initialized_ = false;
    reserved_mask_.store(0U, std::memory_order_release);
    active_voices_.store(0U, std::memory_order_release);
    for (VoiceState& voice : voices_) {
        voice = {};
    }
}

bool AudioSystem::enqueue(const Command& command) noexcept
{
    const core::u32 tail = command_tail_.load(std::memory_order_relaxed);
    const core::u32 next_tail = (tail + 1U) % kCommandQueueCapacity;
    if (next_tail == command_head_.load(std::memory_order_acquire)) {
        return false;
    }
    command_queue_[tail] = command;
    command_tail_.store(next_tail, std::memory_order_release);
    return true;
}

bool AudioSystem::reserve_voice(VoiceHandle& handle) noexcept
{
    for (core::u32 index = 0U; index < kMaxVoices; ++index) {
        const core::u64 bit = core::u64{1} << index;
        core::u64 mask = reserved_mask_.load(std::memory_order_relaxed);
        while ((mask & bit) == 0U) {
            if (reserved_mask_.compare_exchange_weak(mask,
                                                     mask | bit,
                                                     std::memory_order_acquire,
                                                     std::memory_order_relaxed)) {
                core::u32 generation = generations_[index].load(std::memory_order_relaxed) + 1U;
                if (generation == 0U) {
                    generation = 1U;
                }
                generations_[index].store(generation, std::memory_order_release);
                handle = {index, generation};
                return true;
            }
        }
    }
    return false;
}

void AudioSystem::release_reservation(core::u32 index) noexcept
{
    if (index < kMaxVoices) {
        reserved_mask_.fetch_and(~(core::u64{1} << index), std::memory_order_release);
    }
}

core::Status AudioSystem::play_tone(const ProceduralTone& tone, VoiceHandle& handle) noexcept
{
    handle = {};
    if (!initialized_) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!validate_tone(tone)) {
        return invalid();
    }

    VoiceHandle reserved_handle{};
    if (!reserve_voice(reserved_handle)) {
        return core::Status{core::ErrorCode::allocation_failed};
    }
    const Command command{
        .type = CommandType::play,
        .index = reserved_handle.index,
        .generation = reserved_handle.generation,
        .tone = tone,
        .value = 0.0F,
    };
    if (!enqueue(command)) {
        release_reservation(reserved_handle.index);
        return core::Status{core::ErrorCode::allocation_failed};
    }
    handle = reserved_handle;
    return core::Status{};
}

core::Status AudioSystem::stop(VoiceHandle handle) noexcept
{
    if (!initialized_) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!handle.valid() ||
        generations_[handle.index].load(std::memory_order_acquire) != handle.generation) {
        return invalid();
    }
    const Command command{
        .type = CommandType::stop,
        .index = handle.index,
        .generation = handle.generation,
    };
    return enqueue(command) ? core::Status{} : core::Status{core::ErrorCode::allocation_failed};
}

core::Status AudioSystem::stop_all() noexcept
{
    if (!initialized_) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    const Command command{.type = CommandType::stop_all};
    return enqueue(command) ? core::Status{} : core::Status{core::ErrorCode::allocation_failed};
}

core::Status AudioSystem::set_master_gain(core::f32 gain) noexcept
{
    if (!initialized_) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!finite(gain) || gain < 0.0F || gain > 1.0F) {
        return invalid();
    }
    const Command command{
        .type = CommandType::set_master_gain,
        .value = gain,
    };
    return enqueue(command) ? core::Status{} : core::Status{core::ErrorCode::allocation_failed};
}

core::Status AudioSystem::update(core::f32 delta_seconds) noexcept
{
    if (!initialized_) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (!finite(delta_seconds) || delta_seconds < 0.0F) {
        return invalid();
    }
    return core::Status{};
}

void AudioSystem::consume_commands() noexcept
{
    core::u32 head = command_head_.load(std::memory_order_relaxed);
    const core::u32 tail = command_tail_.load(std::memory_order_acquire);
    while (head != tail) {
        const Command command = command_queue_[head];
        head = (head + 1U) % kCommandQueueCapacity;
        command_head_.store(head, std::memory_order_release);

        if (command.index >= kMaxVoices && command.type != CommandType::stop_all &&
            command.type != CommandType::set_master_gain) {
            continue;
        }
        switch (command.type) {
        case CommandType::play: {
            VoiceState& voice = voices_[command.index];
            if (generations_[command.index].load(std::memory_order_acquire) != command.generation) {
                release_reservation(command.index);
                continue;
            }
            voice.active = true;
            voice.generation = command.generation;
            voice.tone = command.tone;
            voice.phase = 0.0F;
            voice.elapsed_seconds = 0.0F;
            active_voices_.fetch_add(1U, std::memory_order_release);
            break;
        }
        case CommandType::stop: {
            VoiceState& voice = voices_[command.index];
            if (voice.active && voice.generation == command.generation) {
                voice.active = false;
                active_voices_.fetch_sub(1U, std::memory_order_release);
                release_reservation(command.index);
            }
            break;
        }
        case CommandType::stop_all:
            for (core::u32 index = 0U; index < kMaxVoices; ++index) {
                if (voices_[index].active) {
                    voices_[index].active = false;
                    release_reservation(index);
                }
            }
            active_voices_.store(0U, std::memory_order_release);
            break;
        case CommandType::set_master_gain:
            master_gain_ = command.value;
            break;
        }
    }
}

void AudioSystem::mix_voice(core::u32 voice_index,
                            core::f32* samples,
                            core::u32 frames) noexcept
{
    VoiceState& voice = voices_[voice_index];
    const core::f32 left_pan = std::cos((voice.tone.pan + 1.0F) * pi * 0.25F);
    const core::f32 right_pan = std::sin((voice.tone.pan + 1.0F) * pi * 0.25F);
    const core::f32 phase_increment = two_pi * voice.tone.frequency_hz * voice.tone.pitch /
                                       static_cast<core::f32>(configuration_.sample_rate);
    const core::f32 elapsed_increment =
        1.0F / static_cast<core::f32>(configuration_.sample_rate);

    for (core::u32 frame = 0U; frame < frames && voice.active; ++frame) {
        const core::f32 value = std::sin(voice.phase) * voice.tone.gain * master_gain_;
        samples[frame * kChannelCount] += value * left_pan;
        samples[frame * kChannelCount + 1U] += value * right_pan;
        voice.phase += phase_increment;
        if (voice.phase >= two_pi || voice.phase <= -two_pi) {
            voice.phase = std::fmod(voice.phase, two_pi);
        }
        voice.elapsed_seconds += elapsed_increment;
        if (!voice.tone.loop && voice.elapsed_seconds >= voice.tone.duration_seconds) {
            voice.active = false;
            active_voices_.fetch_sub(1U, std::memory_order_release);
            release_reservation(voice_index);
        }
    }
}

void AudioSystem::mix(core::f32* samples, core::u32 frames) noexcept
{
    if (samples == nullptr || frames == 0U) {
        return;
    }
    consume_commands();
    std::fill_n(samples, static_cast<core::usize>(frames) * kChannelCount, 0.0F);
    for (core::u32 index = 0U; index < kMaxVoices; ++index) {
        if (voices_[index].active) {
            mix_voice(index, samples, frames);
        }
    }
    const core::usize sample_count = static_cast<core::usize>(frames) * kChannelCount;
    for (core::usize index = 0U; index < sample_count; ++index) {
        samples[index] = std::clamp(samples[index], -1.0F, 1.0F);
    }
    frames_mixed_.fetch_add(frames, std::memory_order_relaxed);
}

void AudioSystem::render_callback(void* user_data,
                                  core::f32* samples,
                                  core::u32 frames) noexcept
{
    if (user_data != nullptr) {
        static_cast<AudioSystem*>(user_data)->mix(samples, frames);
    }
}

core::Status AudioSystem::render_for_testing(
    std::span<core::f32> interleaved_samples) noexcept
{
    if (!initialized_) {
        return core::Status{core::ErrorCode::not_initialized};
    }
    if (interleaved_samples.empty() ||
        interleaved_samples.size() % static_cast<core::usize>(kChannelCount) != 0U ||
        interleaved_samples.size() / kChannelCount >
            static_cast<core::usize>(std::numeric_limits<core::u32>::max())) {
        return invalid();
    }
    mix(interleaved_samples.data(),
        static_cast<core::u32>(interleaved_samples.size() / kChannelCount));
    return core::Status{};
}

AudioMetrics AudioSystem::metrics() const noexcept
{
    return {
        .initialized = initialized_,
        .available = backend_.available,
        .backend = backend_.name == nullptr ? std::string_view{} : std::string_view{backend_.name},
        .active_voices = active_voices_.load(std::memory_order_acquire),
        .underruns = underruns_.load(std::memory_order_relaxed),
        .frames_mixed = frames_mixed_.load(std::memory_order_relaxed),
        .frames_played = frames_played_.load(std::memory_order_relaxed),
        .memory_reserved = memory_usage_bytes(),
    };
}

core::usize AudioSystem::memory_usage_bytes() const noexcept
{
    return sizeof(*this);
}

} // namespace gameengine::audio
