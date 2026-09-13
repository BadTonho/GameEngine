# GameEngine

GameEngine is a public, modular C++ game engine designed to pursue high visual quality with efficient use of CPU, GPU, RAM, VRAM, storage and startup time.

The engine is currently in Phase 0: public foundation. The repository contains the initial CMake build, a minimal C++ runtime, and tests. Rendering, windowing, Vulkan and asset processing are planned for later phases.

## Project direction

The project follows these principles:

- C++20 for the core and runtime;
- a small C ABI for binary plugins and cross-language integration;
- Rust for offline asset and project tooling;
- optional Lua for gameplay scripting;
- Slang for offline shader compilation;
- Vulkan as the first graphics backend;
- explicit ownership and measured resource costs;
- separate runtime, editor and offline tooling;
- optional features with cost proportional to their use.

Read [Ideia.md](Ideia.md) for the long-term vision and [ROADMAP.md](ROADMAP.md) for the implementation plan.

## Version

The project is currently at version `0.0.1`. It is an early development foundation, and no public engine API is considered stable yet.

## Current requirements

- Git;
- CMake 3.25 or newer;
- a C++20 compiler;
- Windows: Visual Studio 2022 with the Desktop development with C++ workload;
- Linux: GCC or Clang and a Make-compatible build tool.

The current code does not require Vulkan, Slang, Rust or Lua.

## Configure and build

List the available presets for the current host:

```sh
cmake --list-presets
```

Windows with Visual Studio:

```sh
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Linux with GCC:

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

Linux with Clang:

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

Linux with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset linux-gcc-sanitizers
cmake --build --preset linux-gcc-sanitizers
ctest --preset linux-gcc-sanitizers
```

The runtime executable is created as `gameengine_runtime`. The test target is `gameengine_tests` and is registered with CTest.

## Repository layout

```text
src/       Runtime and engine source code
tests/     Tests without an external testing framework
docs/      Future architecture and user documentation
tools/     Future offline tooling
editor/    Future editor code
```

Only directories containing source files are created while they are needed. This keeps the repository navigable and avoids empty or speculative structure.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. All contributions must preserve the runtime/editor/tooling boundaries and must not add secrets or machine-specific data to the public repository.

## License

GameEngine is distributed under the [MIT License](LICENSE).
