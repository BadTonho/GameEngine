#include "engine/audio/audio.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <new>
#include <thread>

namespace gameengine::audio::detail {

namespace {

struct WasapiState final {
    static constexpr core::u32 ring_frames = kFramesPerBuffer * 4U;

    BackendCallbacks callbacks{};
    IAudioClient* audio_client = nullptr;
    IAudioRenderClient* render_client = nullptr;
    HANDLE event_handle = nullptr;
    UINT32 buffer_frames = 0U;
    std::atomic<bool> stop_requested{false};
    std::thread worker{};
    std::array<core::f32, ring_frames * kChannelCount> ring_buffer{};
    core::u32 ring_read = 0U;
    core::u32 ring_write = 0U;
    core::u32 ring_count = 0U;
    bool com_initialized = false;

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

    void consume_ring(BYTE* destination, core::u32 frames) noexcept
    {
        core::u32 copied = 0U;
        while (copied < frames) {
            const core::u32 contiguous = std::min(frames - copied, ring_frames - ring_read);
            std::memcpy(destination + copied * kChannelCount * sizeof(core::f32),
                        ring_buffer.data() + ring_read * kChannelCount,
                        static_cast<core::usize>(contiguous) * kChannelCount * sizeof(core::f32));
            ring_read = (ring_read + contiguous) % ring_frames;
            ring_count -= contiguous;
            copied += contiguous;
        }
    }
};

void release_state(WasapiState& state) noexcept
{
    state.stop_requested.store(true, std::memory_order_release);
    if (state.event_handle != nullptr) {
        SetEvent(state.event_handle);
    }
    if (state.worker.joinable()) {
        state.worker.join();
    }
    if (state.audio_client != nullptr) {
        static_cast<void>(state.audio_client->Stop());
    }
    if (state.event_handle != nullptr) {
        CloseHandle(state.event_handle);
        state.event_handle = nullptr;
    }
    if (state.render_client != nullptr) {
        state.render_client->Release();
        state.render_client = nullptr;
    }
    if (state.audio_client != nullptr) {
        state.audio_client->Release();
        state.audio_client = nullptr;
    }
    if (state.com_initialized) {
        CoUninitialize();
        state.com_initialized = false;
    }
}

void audio_thread(WasapiState* state) noexcept
{
    while (!state->stop_requested.load(std::memory_order_acquire)) {
        const DWORD wait_result = WaitForSingleObject(state->event_handle, 100U);
        if (state->stop_requested.load(std::memory_order_acquire)) {
            break;
        }
        if (wait_result != WAIT_OBJECT_0) {
            continue;
        }

        UINT32 padding = 0U;
        if (FAILED(state->audio_client->GetCurrentPadding(&padding))) {
            continue;
        }
        UINT32 available_frames = state->buffer_frames - std::min(padding, state->buffer_frames);
        while (available_frames > 0U &&
               !state->stop_requested.load(std::memory_order_acquire)) {
            const UINT32 frames = std::min(available_frames, kFramesPerBuffer);
            state->fill_ring(frames);
            BYTE* data = nullptr;
            if (FAILED(state->render_client->GetBuffer(frames, &data))) {
                break;
            }
            state->consume_ring(data, frames);
            if (FAILED(state->render_client->ReleaseBuffer(frames, 0U))) {
                break;
            }
            if (state->callbacks.frames_played != nullptr) {
                state->callbacks.frames_played->fetch_add(frames, std::memory_order_relaxed);
            }
            available_frames -= frames;
        }
    }
}

[[nodiscard]] bool initialize_device(WasapiState& state) noexcept
{
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
        return false;
    }
    state.com_initialized = SUCCEEDED(result);

    IMMDeviceEnumerator* enumerator = nullptr;
    result = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                              nullptr,
                              CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator),
                              reinterpret_cast<void**>(&enumerator));
    if (FAILED(result) || enumerator == nullptr) {
        return false;
    }

    IMMDevice* device = nullptr;
    result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    enumerator->Release();
    if (FAILED(result) || device == nullptr) {
        return false;
    }

    result = device->Activate(__uuidof(IAudioClient),
                              CLSCTX_ALL,
                              nullptr,
                              reinterpret_cast<void**>(&state.audio_client));
    device->Release();
    if (FAILED(result) || state.audio_client == nullptr) {
        return false;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = kChannelCount;
    format.nSamplesPerSec = kSampleRate;
    format.wBitsPerSample = 32U;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * sizeof(core::f32));
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    result = state.audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                             AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                             0,
                                             0,
                                             &format,
                                             nullptr);
    if (FAILED(result)) {
        return false;
    }
    result = state.audio_client->GetService(__uuidof(IAudioRenderClient),
                                             reinterpret_cast<void**>(&state.render_client));
    if (FAILED(result) || state.render_client == nullptr) {
        return false;
    }
    state.event_handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (state.event_handle == nullptr ||
        FAILED(state.audio_client->SetEventHandle(state.event_handle)) ||
        FAILED(state.audio_client->GetBufferSize(&state.buffer_frames)) ||
        FAILED(state.audio_client->Start())) {
        return false;
    }
    return true;
}

} // namespace

core::Status initialize_backend(AudioBackend& backend,
                                const AudioConfiguration& configuration,
                                const BackendCallbacks& callbacks) noexcept
{
    backend = {};
    backend.name = "wasapi";

    auto* state = new (std::nothrow) WasapiState{};
    if (state == nullptr) {
        return core::Status{};
    }
    state->callbacks = callbacks;
    if (!initialize_device(*state)) {
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
    auto* state = static_cast<WasapiState*>(backend.state);
    if (state != nullptr) {
        release_state(*state);
        delete state;
    }
    backend = {};
}

} // namespace gameengine::audio::detail
