#pragma once

#include <cstddef>
#include <string_view>

#include "engine/core/status.hpp"
#include "engine/core/types.hpp"
#include "engine/input/input.hpp"
#include "engine/scene/scene.hpp"

namespace gameengine::scripting {

// Optional Lua gameplay scripting boundary. All Lua headers, VM state and allocators stay
// private to scripting.cpp so nothing leaks into runtime, scene, RHI or C ABI headers.

inline constexpr core::usize default_memory_limit_bytes = 1024U * 1024U;
inline constexpr core::usize minimum_memory_limit_bytes = 32U * 1024U;
inline constexpr core::u32 default_max_instructions_per_call = 100'000U;
inline constexpr core::u32 default_max_named_entities = 64U;
inline constexpr core::usize max_entity_name_length = 63U;

struct ScriptConfig final {
    core::usize memory_limit_bytes = default_memory_limit_bytes;
    core::u32 max_instructions_per_call = default_max_instructions_per_call;
    core::u32 max_named_entities = default_max_named_entities;
    bool sandbox = true;
};

struct ScriptMetrics final {
    core::usize memory_bytes = 0;
    core::usize peak_memory_bytes = 0;
    core::u64 allocation_count = 0;
    core::u64 start_calls = 0;
    core::u64 update_calls = 0;
    core::u32 named_entity_count = 0;
    bool initialized = false;
    bool script_loaded = false;
};

[[nodiscard]] core::Status validate_config(const ScriptConfig& config) noexcept;

// Owns one isolated Lua VM with bounded memory, instruction-count timeout protection and
// optional library sandboxing. Not thread-safe: drive a runtime from one thread at a time.
class ScriptRuntime final {
public:
    ScriptRuntime() noexcept = default;
    ~ScriptRuntime() noexcept;

    ScriptRuntime(const ScriptRuntime&) = delete;
    ScriptRuntime& operator=(const ScriptRuntime&) = delete;

    // Allocates and configures the Lua VM. Expensive: do not call in per-frame hot paths.
    [[nodiscard]] core::Status initialize(const ScriptConfig& config = {}) noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool initialized() const noexcept { return impl_ != nullptr; }

    // Compiles and evaluates a UTF-8 Lua source chunk in text-only mode (binary bytecode is
    // rejected as untrusted input).
    [[nodiscard]] core::Status load_script(
        std::string_view source,
        std::string_view chunk_name = "gameplay.lua") noexcept;

    // Binds a name used by Engine.findEntity(name) in Lua scripts. Capacity is reserved in
    // initialize().
    [[nodiscard]] core::Status register_entity(std::string_view name,
                                               scene::Entity entity) noexcept;
    [[nodiscard]] core::Status unregister_entity(std::string_view name) noexcept;
    void clear_entities() noexcept;

    // Invokes global start() if present. Rejects non-function globals named 'start'.
    [[nodiscard]] core::Status call_start(
        scene::Scene& scene,
        const input::InputState* input = nullptr) noexcept;

    // Invokes global update(dt) if present. Rejects negative or non-finite delta_seconds.
    [[nodiscard]] core::Status call_update(
        scene::Scene& scene,
        core::f32 delta_seconds,
        const input::InputState* input = nullptr) noexcept;

    [[nodiscard]] std::string_view last_error() const noexcept;
    [[nodiscard]] ScriptMetrics metrics() const noexcept;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace gameengine::scripting
