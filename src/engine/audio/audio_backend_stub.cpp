#include "engine/audio/audio_backend.hpp"

namespace gameengine::audio::detail {

core::Status initialize_backend(AudioBackend& backend,
                                const AudioConfiguration&,
                                const BackendCallbacks&) noexcept
{
    backend = {};
    backend.name = "stub";
    backend.available = false;
    return core::Status{};
}

void shutdown_backend(AudioBackend& backend) noexcept
{
    backend = {};
}

} // namespace gameengine::audio::detail
