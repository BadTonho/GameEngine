#pragma once

#include <string_view>

#include "engine/core/types.hpp"

namespace gameengine::core {

enum class LogLevel : u8 {
    debug = 0,
    info,
    warning,
    error,
};

void log(LogLevel level, std::string_view message) noexcept;

} // namespace gameengine::core

#ifndef NDEBUG
#include <cassert>
#define GAMEENGINE_ASSERT(condition) assert(condition)
#else
#define GAMEENGINE_ASSERT(condition) ((void)sizeof(condition))
#endif
