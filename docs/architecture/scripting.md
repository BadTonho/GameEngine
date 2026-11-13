# Scripting Module Architecture

## Overview

The optional gameplay scripting module (`gameengine_scripting`) integrates PUC-Rio **Lua 5.4.8** behind a strict C++ PIMPL boundary so Lua remains completely optional at both build time and runtime.

Projects that do not require gameplay scripting can exclude it at compile time with `-DGAMEENGINE_BUILD_LUA=OFF`, leaving zero Lua VM code, symbols, or dependencies in exported binaries.

## Build Targets and Encapsulation

- **`GAMEENGINE_BUILD_LUA`** (`CMakeLists.txt`, default `OFF`; enabled in development `CMakePresets.json`): controls whether Lua and `gameengine_scripting` are built and linked into `gameengine_runtime`.
- **`gameengine_lua_vm`**: static C99 library built from official Lua 5.4.8 source (`FetchContent` with pinned SHA-256 verification), excluding CLI entry points (`lua.c`, `luac.c`). Compiled without the engine's C++ `/WX` / `-Werror` flags so third-party C warnings never leak into engine targets.
- **`gameengine_scripting`**: static C++20 library compiling `src/engine/scripting/scripting.cpp`. Public header `src/engine/scripting/scripting.hpp` exposes only engine types (`core::Status`, `scene::Scene`, `scene::Entity`, `input::InputState`) and never includes Lua headers.
- **`ScriptRuntime` lifetime & exceptions**: Engine C++ targets compile without C++ exceptions or RTTI (`/EHs-c- /GR-`, `-fno-exceptions -fno-rtti`). Lua uses `setjmp`/`longjmp` internally across `lua_pcall`/`luaL_error`, so all Lua C callbacks in `scripting.cpp` keep no non-trivial C++ destructors live on stack frames that may longjmp.

## Sandboxing and Determinism Guards

Untrusted or buggy scripts are constrained explicitly:

1. **Bounded custom allocator (`lua_Alloc`)**: tracks live `memory_bytes`, `peak_memory_bytes`, and `allocation_count` inside `ScriptRuntimeState`. Allocations exceeding `ScriptConfig::memory_limit_bytes` (default `1 MiB`, minimum `32 KiB`) fail deterministically with `ErrorCode::script_memory_exceeded`.
2. **Instruction-count timeout (`LUA_MASKCOUNT`)**: interrupts infinite loops or runaway callbacks when `ScriptConfig::max_instructions_per_call` (default `100,000` instructions) is exhausted, re-arming every instruction inside the hook so Lua `pcall` cannot mask termination (`ErrorCode::script_instruction_limit_exceeded`).
3. **Text-only chunk loading**: `luaL_loadbufferx(..., "t")` rejects precompiled Lua bytecode (`\x1bLua`) as untrusted binary input.
4. **Restricted standard library (`sandbox = true`)**: opens only `base`, `coroutine`, `table`, `string`, `math`, and `utf8`, while removing `dofile`, `loadfile`, `load`, `os`, `io`, `debug`, and `package`.

## Script API Surface

Registered engine bindings operate exclusively through generation-checked handles:

- `Engine.findEntity(name)` -> returns `GameEngine.Entity` userdata or `nil`
- `Engine.isAlive(entity)` -> `boolean`
- `Engine.log(message)` -> writes an info log entry (bounded stack buffer)
- `Input.keyDown(keyName)` -> queries active `input::InputState`
- `entity:isAlive()`, `entity:getPosition()`, `entity:setPosition(x, y, z)`, `entity:move(dx, dy, dz)`, `entity:getScale()`, `entity:setScale(x, y, z)`, `entity:rotateY(radians)`

Destroyed or invalid entity handles fail explicitly rather than dereferencing stale scene storage. Scene transforms are flushed once after script hooks finish modifying entities.

## Runtime Modes and Verification

- `gameengine_runtime --lua-smoke-test`: executes headless VM startup, entity registration, `start()`, `update(dt)`, metric capture, and clean shutdown.
- `gameengine_runtime --smoke-test --lua-demo`: drives `BootstrapCube` in the Vulkan runtime via a deterministic Lua `update(dt)` hook.
- Unit tests (`tests/scripting_tests.cpp` -> `gameengine_scripting`): verify config validation, VM lifecycle, transform/input bindings, generation-checked destroyed entities, bytecode rejection, sandbox library removal, instruction budget abort (including nested `pcall`), memory limit enforcement, and named-entity capacity limits.

## Measured Local Baseline and Removal Cost

Measured on the local Windows x64 (`windows-msvc-release`) development baseline with `gameengine_scripting_benchmark`:

- **Script update throughput**: `433 µs` for 1,000 `update(dt)` calls (~0.43 µs/call) and `4,771 µs` for 10,000 `update(dt)` calls (~0.48 µs/call).
- **VM memory footprint**: `15,656` bytes live / `16,280` bytes peak across both 1,000 and 10,000 `update(dt)` calls (zero steady-state growth during per-frame execution).
- **Binary size (`gameengine_runtime.exe`, Release)**: `1,403,392` bytes (`GAMEENGINE_BUILD_LUA=ON`) vs `1,164,800` bytes (`GAMEENGINE_BUILD_LUA=OFF`) — excluding Lua removes `238,592` bytes (`~233 KiB`) and leaves zero Lua symbols or VM allocations.
