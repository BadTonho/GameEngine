#include "engine/audio/audio.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

namespace {

using namespace gameengine;

void run_case(core::u32 voice_count)
{
    audio::AudioConfiguration configuration{};
    configuration.enable_output = false;
    audio::AudioSystem system;
    if (!system.initialize(configuration)) {
        std::printf("audio voices=%u unavailable\n", voice_count);
        return;
    }

    std::vector<audio::VoiceHandle> handles(voice_count);
    for (core::u32 index = 0U; index < voice_count; ++index) {
        static_cast<void>(system.play_tone(
            {220.0F + static_cast<core::f32>(index), 10.0F, 0.01F, 0.0F, 1.0F, true},
            handles[index]));
    }

    std::vector<core::f32> samples(audio::kFramesPerBuffer * audio::kChannelCount);
    const auto start = std::chrono::steady_clock::now();
    for (core::u32 block = 0U; block < 120U; ++block) {
        static_cast<void>(system.render_for_testing(samples));
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start)
                             .count();
    const audio::AudioMetrics metrics = system.metrics();
    std::printf("audio voices=%u mix_ns=%lld active=%u frames=%llu memory=%zu underruns=%llu\n",
                voice_count,
                static_cast<long long>(elapsed),
                metrics.active_voices,
                static_cast<unsigned long long>(metrics.frames_mixed),
                metrics.memory_reserved,
                static_cast<unsigned long long>(metrics.underruns));
}

} // namespace

int main()
{
    run_case(1U);
    run_case(16U);
    run_case(64U);
    return 0;
}
