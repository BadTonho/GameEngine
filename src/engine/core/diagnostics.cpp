#include "engine/core/diagnostics.hpp"

#include <cstdio>

namespace gameengine::core {

namespace {

[[nodiscard]] const char* level_name(LogLevel level) noexcept
{
    switch (level) {
    case LogLevel::debug:
        return "debug";
    case LogLevel::info:
        return "info";
    case LogLevel::warning:
        return "warning";
    case LogLevel::error:
        return "error";
    }

    return "unknown";
}

} // namespace

void log(LogLevel level, std::string_view message) noexcept
{
    std::fputs("[gameengine] [", stderr);
    std::fputs(level_name(level), stderr);
    std::fputs("] ", stderr);
    std::fwrite(message.data(), sizeof(char), message.size(), stderr);
    std::fputc('\n', stderr);
}

} // namespace gameengine::core
