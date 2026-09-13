# GameEngine

[![CI](https://github.com/BadTonho/GameEngine/actions/workflows/ci.yml/badge.svg)](https://github.com/BadTonho/GameEngine/actions/workflows/ci.yml)

A public, modular C++ game engine focused on high visual quality, efficient hardware usage and long-term maintainability.

> **Status: early development — Phase 1A.** The repository currently provides a Linux/X11 platform foundation. It is not ready to create a complete game yet.

## Why this project exists

GameEngine is being designed around a simple principle: projects should pay for the systems they use. The long-term goal is to combine modern rendering quality with controlled RAM, CPU, GPU, VRAM, storage and startup costs.

The project is intended to grow into:

- a lightweight and modular C++ runtime;
- a scalable renderer with Vulkan as its first graphics backend;
- an editor that remains separate from exported games;
- offline tools for asset and project processing;
- optional systems whose cost is explicit and proportional to their use;
- a small C ABI for binary plugins and cross-language integration.

The complete direction is documented in [Idea.md](Idea.md). The implementation sequence is tracked in [ROADMAP.md](ROADMAP.md).

## Current status

Phase 0 is implemented and Phase 1A currently provides the first runtime platform layer:

- CMake 3.25+ project using C++20 without compiler extensions;
- `gameengine_core` with fixed-width types, status values, diagnostics, clock and input state;
- `gameengine_platform` with a native Xlib/X11 Linux backend;
- `gameengine_runtime` executable with a window, event processing and clean shutdown;
- `gameengine_tests` and `gameengine_platform_tests` without an external test framework;
- CTest integration with core, runtime and X11 smoke tests;
- high-warning builds with warnings treated as errors;
- Debug, Release and GCC sanitizer presets;
- GitHub Actions for Windows/MSVC, Linux/GCC, Linux/Clang and ASan/UBSan.

The following are intentionally not part of Phase 1A:

- the functional Windows/Win32 backend;
- Vulkan, RHI or renderer code;
- shaders and asset processing;
- ECS, editor, Lua and Rust tooling;
- physics, audio, networking or gameplay APIs;
- a functional C ABI.

## Technology direction

| Area | Direction | Phase 0 status |
| --- | --- | --- |
| Runtime and core | C++20 | In use |
| Build system | CMake 3.25+ | In use |
| Tests | CTest, no external framework | In use |
| Linux platform | Xlib/X11 | In use in Phase 1A |
| Binary interoperability | Versioned C ABI | Planned |
| Offline tooling | Rust | Planned |
| Gameplay scripting | Optional Lua | Planned |
| Shader source | Slang, compiled offline | Planned |
| First graphics backend | Vulkan | Planned |

Phase 0 has no third-party runtime dependencies. Vulkan, Slang, Rust and Lua are not required to configure or build the current repository.

## Quick start

### Requirements

- Git;
- CMake 3.25 or newer;
- a C++20 compiler;
- Windows: Visual Studio 2022 with the **Desktop development with C++** workload;
- Linux: GCC or Clang, a Make-compatible build tool, `libx11-dev` and `xvfb`.

On Debian or Ubuntu, install the Linux development and virtual-display packages:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends libx11-dev xvfb
```

Clone the repository and enter its root directory:

```sh
git clone https://github.com/BadTonho/GameEngine.git
cd GameEngine
```

List the presets available for the current host:

```sh
cmake --list-presets
```

### Windows with MSVC

Run these commands from a Visual Studio Developer PowerShell or a shell where the MSVC toolchain is available:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

For an optimized build, replace `windows-msvc-debug` with `windows-msvc-release` in all three commands.

### Linux with GCC

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

For an optimized build, replace `linux-gcc-debug` with `linux-gcc-release` in all three commands.

### Linux with Clang

```sh
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

For an optimized build, replace `linux-clang-debug` with `linux-clang-release` in all three commands.

### Linux with sanitizers

The sanitizer preset enables AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset linux-gcc-sanitizers
cmake --build --preset linux-gcc-sanitizers
ctest --preset linux-gcc-sanitizers
```

Presets are host-filtered. Windows shows the Windows presets, while Linux shows the Linux presets.

## Preset matrix

| Preset | Platform | Compiler | Configuration |
| --- | --- | --- | --- |
| `windows-msvc-debug` | Windows | MSVC | Debug |
| `windows-msvc-release` | Windows | MSVC | Release |
| `linux-gcc-debug` | Linux | GCC | Debug |
| `linux-gcc-release` | Linux | GCC | Release |
| `linux-clang-debug` | Linux | Clang | Debug |
| `linux-clang-release` | Linux | Clang | Release |
| `linux-gcc-sanitizers` | Linux | GCC | Debug + ASan/UBSan |

## Build outputs and tests

On Linux, the runtime opens a 1280x720 X11 window and runs until it receives a close request. For an automated smoke test, use `--smoke-test`:

```sh
xvfb-run --auto-servernum ./build/linux-gcc-debug/gameengine_runtime --smoke-test
```

On Windows, the Debug executable is located at:

```text
build/windows-msvc-debug/Debug/gameengine_runtime.exe
```

On Linux, the GCC Debug executable is located at:

```text
build/linux-gcc-debug/gameengine_runtime
```

CTest currently runs three checks on Linux:

1. `gameengine_core`: verifies the core initialization and shutdown lifecycle;
2. `gameengine_runtime_smoke`: verifies that the runtime creates and destroys an X11 window;
3. `gameengine_platform_x11`: verifies the platform lifecycle and idempotent shutdown.

Build directories, compiler output, IDE files and local configuration are excluded by [.gitignore](.gitignore).

## Contributing

The project is being built deliberately and incrementally. Before opening an issue or pull request:

1. read [Idea.md](Idea.md) and the current [ROADMAP.md](ROADMAP.md);
2. check whether the proposed work belongs in the current phase;
3. keep runtime, editor and offline tooling boundaries separate;
4. run the relevant CMake build and CTest preset;
5. keep the diff free of generated artifacts, credentials, personal data and machine-specific paths.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the complete contribution workflow and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for community expectations.

## Version and compatibility

The current version is `0.0.1`. The project is in early development and does not promise API or asset-format stability yet. Public compatibility contracts will be defined before a stable release.

## License

GameEngine is distributed under the [MIT License](LICENSE).
