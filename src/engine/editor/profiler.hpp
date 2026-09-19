#pragma once

#include "engine/core/types.hpp"
#include "engine/renderer/renderer_metrics.hpp"

namespace gameengine::editor {

struct MemorySnapshot final {
    bool current_available = false;
    bool peak_available = false;
    core::u64 current_bytes = 0;
    core::u64 peak_bytes = 0;
};

class EditorProfiler final {
public:
    void set_startup_nanoseconds(core::u64 value) noexcept { startup_nanoseconds_ = value; }
    void update_frame(const renderer::metrics::FrameTimingReport& report) noexcept;
    void sample_memory() noexcept;

    [[nodiscard]] bool has_frame() const noexcept { return has_frame_; }
    [[nodiscard]] const renderer::metrics::FrameTimingReport& frame() const noexcept
    {
        return frame_;
    }
    [[nodiscard]] core::u64 startup_nanoseconds() const noexcept
    {
        return startup_nanoseconds_;
    }
    [[nodiscard]] const MemorySnapshot& memory() const noexcept { return memory_; }

private:
    renderer::metrics::FrameTimingReport frame_{};
    MemorySnapshot memory_{};
    core::u64 startup_nanoseconds_ = 0;
    bool has_frame_ = false;
};

} // namespace gameengine::editor
