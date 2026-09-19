#pragma once

#include <array>
#include <string_view>

#include "engine/core/diagnostics.hpp"
#include "engine/core/status.hpp"
#include "engine/core/types.hpp"

namespace gameengine::editor {

struct ConsoleMessageView final {
    core::LogLevel level = core::LogLevel::info;
    std::string_view text{};
};

class ConsoleBuffer final {
public:
    static constexpr core::u32 capacity = 64U;
    static constexpr core::u32 message_capacity = 192U;

    void push(core::LogLevel level, std::string_view text) noexcept;
    void clear() noexcept;

    [[nodiscard]] core::u32 size() const noexcept { return size_; }
    [[nodiscard]] ConsoleMessageView message(core::u32 index) const noexcept;

private:
    struct Message final {
        core::LogLevel level = core::LogLevel::info;
        std::array<char, message_capacity> text{};
        core::u32 length = 0;
    };

    std::array<Message, capacity> messages_{};
    core::u32 first_ = 0;
    core::u32 size_ = 0;
};

void record_console_message(ConsoleBuffer& console,
                            core::LogLevel level,
                            std::string_view text) noexcept;
void record_console_status(ConsoleBuffer& console,
                           core::LogLevel level,
                           std::string_view prefix,
                           core::Status status) noexcept;

} // namespace gameengine::editor
