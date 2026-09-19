#include "engine/audio/audio.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace gameengine;

bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "audio test failure: %s\n", message);
    }
    return condition;
}

bool finite_samples(const std::vector<core::f32>& samples)
{
    for (const core::f32 sample : samples) {
        if (!std::isfinite(sample) || sample < -1.0F || sample > 1.0F) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    bool passed = true;

    audio::AudioConfiguration configuration{};
    configuration.enable_output = false;
    audio::AudioSystem system;
    passed = expect(system.initialize(configuration).ok(), "initialize") && passed;
    passed = expect(system.is_initialized() && !system.available() &&
                        system.backend_name() == "stub",
                    "stub backend") &&
             passed;

    audio::ProceduralTone tone{};
    tone.duration_seconds = 0.1F;
    tone.pan = 0.0F;
    audio::VoiceHandle handle{};
    passed = expect(system.play_tone(tone, handle).ok() && handle.valid(), "play tone") && passed;

    const core::usize memory_before = system.memory_usage_bytes();
    std::vector<core::f32> first_block(audio::kFramesPerBuffer * audio::kChannelCount);
    passed = expect(system.render_for_testing(first_block).ok() && finite_samples(first_block),
                    "render tone") &&
             passed;
    const audio::AudioMetrics first_metrics = system.metrics();
    passed = expect(first_metrics.frames_mixed == audio::kFramesPerBuffer &&
                        first_metrics.active_voices == 1U,
                    "metrics after render") &&
             passed;
    passed = expect(system.memory_usage_bytes() == memory_before, "render does not allocate") &&
             passed;

    audio::AudioSystem repeat_system;
    passed = expect(repeat_system.initialize(configuration).ok(), "repeat initialize") && passed;
    audio::VoiceHandle repeat_handle{};
    passed = expect(repeat_system.play_tone(tone, repeat_handle).ok(), "repeat play") && passed;
    std::vector<core::f32> repeat_block(first_block.size());
    passed = expect(repeat_system.render_for_testing(repeat_block).ok() &&
                        first_block == repeat_block,
                    "deterministic samples") &&
             passed;

    passed = expect(system.stop(handle).ok(), "stop tone") && passed;
    std::array<core::f32, audio::kFramesPerBuffer * audio::kChannelCount> silent_block{};
    passed = expect(system.render_for_testing(silent_block).ok(), "render stopped tone") && passed;
    for (const core::f32 sample : silent_block) {
        passed = expect(sample == 0.0F, "stopped tone is silent") && passed;
    }

    audio::VoiceHandle recycled_handle{};
    passed = expect(system.play_tone(tone, recycled_handle).ok() &&
                        recycled_handle.index == handle.index &&
                        recycled_handle.generation != handle.generation,
                    "generation protected reuse") &&
             passed;
    passed = expect(system.stop(handle).code == core::ErrorCode::invalid_argument,
                    "stale handle rejected") &&
             passed;

    audio::AudioSystem pan_system;
    passed = expect(pan_system.initialize(configuration).ok(), "pan initialize") && passed;
    audio::VoiceHandle pan_handle{};
    passed = expect(pan_system.play_tone(
                             {440.0F, 0.1F, 0.5F, 1.0F, 1.0F, false}, pan_handle)
                            .ok(),
                    "pan tone") &&
             passed;
    std::vector<core::f32> pan_block(audio::kFramesPerBuffer * audio::kChannelCount);
    passed = expect(pan_system.render_for_testing(pan_block).ok(), "pan render") && passed;
    core::f32 left_energy = 0.0F;
    core::f32 right_energy = 0.0F;
    for (core::u32 frame = 0U; frame < audio::kFramesPerBuffer; ++frame) {
        left_energy += pan_block[frame * audio::kChannelCount] *
                       pan_block[frame * audio::kChannelCount];
        right_energy += pan_block[frame * audio::kChannelCount + 1U] *
                        pan_block[frame * audio::kChannelCount + 1U];
    }
    passed = expect(right_energy > left_energy * 100.0F, "right pan") && passed;
    passed = expect(pan_system.set_master_gain(0.0F).ok(), "master gain") && passed;
    passed = expect(pan_system.render_for_testing(pan_block).ok(), "silent gain render") && passed;
    for (const core::f32 sample : pan_block) {
        passed = expect(sample == 0.0F, "master gain silence") && passed;
    }
    passed = expect(pan_system.update(-1.0F).code == core::ErrorCode::invalid_argument,
                    "negative delta rejected") &&
             passed;

    audio::AudioConfiguration invalid_configuration = configuration;
    invalid_configuration.sample_rate = 44'100U;
    audio::AudioSystem invalid_system;
    passed = expect(invalid_system.initialize(invalid_configuration).code ==
                        core::ErrorCode::invalid_argument,
                    "invalid configuration rejected") &&
             passed;

    audio::AudioSystem queue_system;
    passed = expect(queue_system.initialize(configuration).ok(), "queue initialize") && passed;
    core::u32 queue_failures = 0U;
    for (core::u32 index = 0U; index < audio::kCommandQueueCapacity; ++index) {
        if (!queue_system.set_master_gain(0.5F)) {
            ++queue_failures;
        }
    }
    passed = expect(queue_failures == 1U, "command queue capacity") && passed;
    std::array<core::f32, audio::kFramesPerBuffer * audio::kChannelCount> queue_block{};
    passed = expect(queue_system.render_for_testing(queue_block).ok(), "queue drain") && passed;

    audio::ProceduralTone invalid_tone = tone;
    invalid_tone.pan = 2.0F;
    audio::VoiceHandle invalid_handle{};
    passed = expect(system.play_tone(invalid_tone, invalid_handle).code ==
                        core::ErrorCode::invalid_argument,
                    "invalid tone rejected") &&
             passed;

    audio::AudioSystem voices_system;
    passed = expect(voices_system.initialize(configuration).ok(), "voices initialize") && passed;
    std::array<audio::VoiceHandle, audio::kMaxVoices> handles{};
    for (audio::VoiceHandle& voice_handle : handles) {
        passed = expect(voices_system.play_tone({440.0F, 10.0F, 0.01F, 0.0F, 1.0F, true},
                                                 voice_handle)
                            .ok(),
                        "voice capacity") &&
                 passed;
    }
    audio::VoiceHandle overflow{};
    passed = expect(voices_system.play_tone(tone, overflow).code ==
                        core::ErrorCode::allocation_failed,
                    "voice capacity rejected") &&
             passed;
    std::vector<core::f32> voices_block(audio::kFramesPerBuffer * audio::kChannelCount);
    passed = expect(voices_system.render_for_testing(voices_block).ok() &&
                        voices_system.metrics().active_voices == audio::kMaxVoices,
                    "active voice metrics") &&
             passed;
    passed = expect(voices_system.stop_all().ok(), "stop all") && passed;
    passed = expect(voices_system.render_for_testing(voices_block).ok() &&
                        voices_system.metrics().active_voices == 0U,
                    "stop all applied") &&
             passed;

    system.shutdown();
    repeat_system.shutdown();
    pan_system.shutdown();
    queue_system.shutdown();
    voices_system.shutdown();
    return passed ? 0 : 1;
}
