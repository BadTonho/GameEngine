#pragma once

#include <atomic>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"

namespace gameengine::audio {

struct AudioConfiguration;

namespace detail {

struct BackendCallbacks final {
    void* user_data = nullptr;
    void (*render)(void* user_data, core::f32* samples, core::u32 frames) noexcept = nullptr;
    std::atomic<core::u64>* underruns = nullptr;
    std::atomic<core::u64>* frames_played = nullptr;
};

struct AudioBackend final {
    void* state = nullptr;
    const char* name = "stub";
    bool available = false;
};

[[nodiscard]] core::Status initialize_backend(AudioBackend& backend,
                                               const AudioConfiguration& configuration,
                                               const BackendCallbacks& callbacks) noexcept;
void shutdown_backend(AudioBackend& backend) noexcept;

} // namespace detail
} // namespace gameengine::audio
