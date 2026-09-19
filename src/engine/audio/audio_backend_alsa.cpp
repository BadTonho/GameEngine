#include "engine/audio/audio.hpp"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <new>
#include <thread>

namespace gameengine::audio::detail {

namespace {

struct AlsaState final {
    static constexpr core::u32 ring_frames = kFramesPerBuffer * 4U;

    BackendCallbacks callbacks{};
    snd_pcm_t* pcm = nullptr;
    std::atomic<bool> stop_requested{false};
    std::thread worker{};
    std::array<core::f32, ring_frames * kChannelCount> ring_buffer{};
    core::u32 ring_read = 0U;
    core::u32 ring_write = 0U;
    core::u32 ring_count = 0U;

    void fill_ring(core::u32 requested_frames) noexcept
    {
        while (ring_count < requested_frames) {
            const core::u32 contiguous = std::min(
                std::min(kFramesPerBuffer, requested_frames - ring_count),
                ring_frames - ring_write);
            callbacks.render(callbacks.user_data,
                             ring_buffer.data() + ring_write * kChannelCount,
                             contiguous);
            ring_write = (ring_write + contiguous) % ring_frames;
            ring_count += contiguous;
        }
    }
};

void release_state(AlsaState& state) noexcept
{
    state.stop_requested.store(true, std::memory_order_release);
    if (state.pcm != nullptr) {
        static_cast<void>(snd_pcm_drop(state.pcm));
    }
    if (state.worker.joinable()) {
        state.worker.join();
    }
    if (state.pcm != nullptr) {
        static_cast<void>(snd_pcm_close(state.pcm));
        state.pcm = nullptr;
    }
}

void audio_thread(AlsaState* state) noexcept
{
    while (!state->stop_requested.load(std::memory_order_acquire)) {
        state->fill_ring(kFramesPerBuffer);
        const core::u32 contiguous = std::min(kFramesPerBuffer,
                                              AlsaState::ring_frames - state->ring_read);
        snd_pcm_sframes_t written = snd_pcm_writei(state->pcm,
                                                    state->ring_buffer.data() +
                                                        state->ring_read * kChannelCount,
                                                    contiguous);
        if (written == -EPIPE) {
            if (state->callbacks.underruns != nullptr) {
                state->callbacks.underruns->fetch_add(1U, std::memory_order_relaxed);
            }
            static_cast<void>(snd_pcm_prepare(state->pcm));
            state->ring_count = 0U;
            continue;
        }
        if (written < 0) {
            written = snd_pcm_recover(state->pcm, static_cast<int>(written), 1);
            if (written < 0) {
                break;
            }
            continue;
        }
        state->ring_read = (state->ring_read + static_cast<core::u32>(written)) %
                           AlsaState::ring_frames;
        state->ring_count -= static_cast<core::u32>(written);
        if (state->callbacks.frames_played != nullptr) {
            state->callbacks.frames_played->fetch_add(static_cast<core::u64>(written),
                                                      std::memory_order_relaxed);
        }
    }
}

[[nodiscard]] bool configure_device(AlsaState& state) noexcept
{
    if (snd_pcm_open(&state.pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        return false;
    }

    snd_pcm_hw_params_t* parameters = nullptr;
    if (snd_pcm_hw_params_malloc(&parameters) < 0 || parameters == nullptr) {
        return false;
    }
    bool valid = snd_pcm_hw_params_any(state.pcm, parameters) >= 0;
    valid = valid && snd_pcm_hw_params_set_access(
                         state.pcm, parameters, SND_PCM_ACCESS_RW_INTERLEAVED) >= 0;
    valid = valid && snd_pcm_hw_params_set_format(state.pcm, parameters, SND_PCM_FORMAT_FLOAT_LE) >= 0;
    unsigned int rate = kSampleRate;
    int direction = 0;
    valid = valid && snd_pcm_hw_params_set_rate_near(state.pcm, parameters, &rate, &direction) >= 0;
    valid = valid && rate == kSampleRate;
    valid = valid && snd_pcm_hw_params_set_channels(state.pcm, parameters, kChannelCount) >= 0;
    snd_pcm_uframes_t period_size = kFramesPerBuffer;
    valid = valid && snd_pcm_hw_params_set_period_size_near(
                         state.pcm, parameters, &period_size, &direction) >= 0;
    valid = valid && snd_pcm_hw_params(state.pcm, parameters) >= 0;
    snd_pcm_hw_params_free(parameters);
    if (!valid) {
        return false;
    }
    return snd_pcm_prepare(state.pcm) >= 0;
}

} // namespace

core::Status initialize_backend(AudioBackend& backend,
                                const AudioConfiguration& configuration,
                                const BackendCallbacks& callbacks) noexcept
{
    backend = {};
    backend.name = "alsa";
    auto* state = new (std::nothrow) AlsaState{};
    if (state == nullptr) {
        return core::Status{};
    }
    state->callbacks = callbacks;
    if (!configure_device(*state)) {
        release_state(*state);
        delete state;
        return core::Status{};
    }
    state->worker = std::thread{audio_thread, state};
    backend.state = state;
    backend.available = true;
    static_cast<void>(configuration);
    return core::Status{};
}

void shutdown_backend(AudioBackend& backend) noexcept
{
    auto* state = static_cast<AlsaState*>(backend.state);
    if (state != nullptr) {
        release_state(*state);
        delete state;
    }
    backend = {};
}

} // namespace gameengine::audio::detail
