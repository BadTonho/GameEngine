#include "engine/editor/console.hpp"

#include <algorithm>
#include <cstdio>

namespace gameengine::editor {

void ConsoleBuffer::push(core::LogLevel level, std::string_view text) noexcept
{
    const core::u32 slot = (first_ + size_) % capacity;
    if (size_ == capacity) {
        first_ = (first_ + 1U) % capacity;
    } else {
        ++size_;
    }
    Message& message = messages_[slot];
    message.level = level;
    message.length = static_cast<core::u32>(
        std::min<std::size_t>(text.size(), message_capacity - 1U));
    std::copy_n(text.data(), message.length, message.text.data());
    message.text[message.length] = '\0';
}

void ConsoleBuffer::clear() noexcept
{
    first_ = 0;
    size_ = 0;
}

ConsoleMessageView ConsoleBuffer::message(core::u32 index) const noexcept
{
    if (index >= size_) {
        return {};
    }
    const Message& stored = messages_[(first_ + index) % capacity];
    return {stored.level, std::string_view{stored.text.data(), stored.length}};
}

void record_console_message(ConsoleBuffer& console,
                            core::LogLevel level,
                            std::string_view text) noexcept
{
    console.push(level, text);
    core::log(level, text);
}

void record_console_status(ConsoleBuffer& console,
                           core::LogLevel level,
                           std::string_view prefix,
                           core::Status status) noexcept
{
    if (status) {
        record_console_message(console, level, prefix);
        return;
    }
    char message[256]{};
    const int written = std::snprintf(message,
                                      sizeof(message),
                                      "%.*s: %s",
                                      static_cast<int>(prefix.size()),
                                      prefix.data(),
                                      core::to_string(status.code));
    if (written <= 0) {
        record_console_message(console, level, "operation failed");
        return;
    }
    const std::size_t length = static_cast<std::size_t>(written) < sizeof(message)
        ? static_cast<std::size_t>(written)
        : sizeof(message) - 1U;
    record_console_message(console, level, std::string_view{message, length});
}

} // namespace gameengine::editor
