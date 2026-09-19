#include "engine/editor/profiler.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#endif

namespace gameengine::editor {

namespace {

#if defined(__linux__)
[[nodiscard]] core::u64 parse_kib(std::string_view value) noexcept
{
    core::u64 result = 0;
    bool has_digit = false;
    for (const char character : value) {
        if (character >= '0' && character <= '9') {
            has_digit = true;
            result = result * 10U + static_cast<core::u64>(character - '0');
        } else if (has_digit) {
            break;
        }
    }
    return has_digit ? result * 1024U : 0;
}
#endif

[[nodiscard]] MemorySnapshot query_memory() noexcept
{
    MemorySnapshot result;
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) != 0) {
        result.current_available = true;
        result.peak_available = true;
        result.current_bytes = static_cast<core::u64>(counters.WorkingSetSize);
        result.peak_bytes = static_cast<core::u64>(counters.PeakWorkingSetSize);
    }
#elif defined(__linux__)
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            result.current_bytes = parse_kib(std::string_view{line}.substr(6));
            result.current_available = result.current_bytes != 0;
        } else if (line.rfind("VmHWM:", 0) == 0) {
            result.peak_bytes = parse_kib(std::string_view{line}.substr(6));
            result.peak_available = result.peak_bytes != 0;
        }
    }
#else
    // The editor remains functional when the platform has no process-memory API.
#endif
    return result;
}

} // namespace

void EditorProfiler::update_frame(const renderer::metrics::FrameTimingReport& report) noexcept
{
    frame_ = report;
    has_frame_ = true;
}

void EditorProfiler::sample_memory() noexcept
{
    memory_ = query_memory();
}

} // namespace gameengine::editor
