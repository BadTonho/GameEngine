# GameEngine

[![CI](https://github.com/BadTonho/GameEngine/actions/workflows/ci.yml/badge.svg)](https://github.com/BadTonho/GameEngine/actions/workflows/ci.yml)

A public, modular C++ game engine focused on high visual quality, efficient hardware usage and long-term maintainability.

> **Status: early development — Phase 6 scene foundation in use.** The repository provides a Vulkan rendering foundation with an offline Slang shader pipeline, a deterministic procedural textured cube, versioned asset formats/tools and an internal scene graph. It is not ready to create a complete game yet.

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

Phase 0 through Phase 4 are implemented in the current procedural scope. Phase 5 now adds the first prepared-asset foundation:

- CMake 3.25+ project using C++20 without compiler extensions;
- `gameengine_core` with fixed-width types, status values, diagnostics, clock and input state;
- `gameengine_platform` with a native Xlib/X11 Linux backend;
- `gameengine_renderer` with a Vulkan RHI, X11 surface backend and typed resource handles;
- `gameengine_runtime` executable with a window, Vulkan device, swapchain and indexed cube;
- versioned `.gemesh`, `.getex`, `.gemat` and `.gescene` containers with 64-byte headers, FNV-1a IDs and 16-byte aligned chunks;
- an internal zero-copy C++ asset reader that validates containers and payload views without interpreting glTF or image files;
- an optional Rust workspace under `tools/` with deterministic packers, a minimal JSON glTF importer, synthetic fixtures, validation and incremental cache support;
- an internal scene layer with generation-checked entity handles, sparse-set component pools, TRS hierarchy, retrocompatible `.gescene` graph serialization and synthetic benchmarks;
- Vulkan vertex/index buffers, procedural RGBA8 texture, sampler, material descriptors, depth resources, graphics pipelines and staging uploads;
- internal PBR material with directional lighting and deterministic HDR tone mapping;
- internal `Vec3`/`Mat4` math with right-handed Vulkan-compatible perspective and look-at transforms;
- generation-checked handles and fence-based deferred resource destruction;
- Vulkan object names and command labels through `VK_EXT_debug_utils`;
- `gameengine_tests`, `gameengine_asset_tests`, `gameengine_rhi_tests`, `gameengine_math_tests`, `gameengine_platform_tests` and shader pipeline tests without an external test framework;
- CTest integration with core, assets, RHI, math, runtime, Vulkan, shader pipeline and X11 smoke tests;
- high-warning builds with warnings treated as errors;
- Debug, Release and GCC sanitizer presets;
- GitHub Actions for Windows/MSVC, Linux/GCC, Linux/Clang and ASan/UBSan.

The following are intentionally outside the current procedural renderer scope:

- prepared mesh/assets loading;
- asset-file texture/material loading;
- runtime shader file loading, automatic polling and editor integration;
- prepared asset runtime integration, editor, Lua and a final ECS storage choice;
- physics, audio, networking or gameplay APIs;
- a functional C ABI.

## Technology direction

| Area | Direction | Current status |
| --- | --- | --- |
| Runtime and core | C++20 | In use |
| Build system | CMake 3.25+ | In use |
| Tests | CTest, no external framework | In use |
| Linux platform | Xlib/X11 | In use |
| Binary interoperability | Versioned C ABI | Planned |
| Offline tooling | Rust | Phase 5 foundation in use |
| Gameplay scripting | Optional Lua | Planned |
| Shader source | Slang, compiled offline | Phase 4 in use |
| First graphics backend | Vulkan | In use |

Vulkan, X11 and Mesa are system dependencies for the Linux rendering build. Slang is required only to regenerate offline shader artifacts; it is never a runtime dependency. Rust is optional and is required only for the offline asset tools; Lua is not required.

## Quick start

### Requirements

- Git;
- CMake 3.25 or newer;
- a C++20 compiler;
- Windows: Visual Studio 2022 with the **Desktop development with C++** workload;
- Linux: GCC or Clang, a Make-compatible build tool, `libx11-dev`, `libvulkan-dev`, `mesa-vulkan-drivers`, `vulkan-validationlayers`, `vulkan-tools` and `xvfb`.

On Debian or Ubuntu, install the Linux development and virtual-display packages:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends libx11-dev libvulkan-dev mesa-vulkan-drivers vulkan-validationlayers vulkan-tools xvfb
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

The sanitizer test preset disables LeakSanitizer because the Mesa/DBus stack used by Xvfb keeps process-lifetime allocations outside the engine. AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.

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

CTest currently runs nine checks on Linux and ten on Windows when Vulkan is enabled:

1. `gameengine_core`: verifies the core initialization and shutdown lifecycle;
2. `gameengine_assets`: validates synthetic versioned asset containers and zero-copy views;
3. `gameengine_rhi`: verifies RHI lifecycle error handling and Vulkan utility policies;
4. `gameengine_shaders`: verifies SPIR-V layout, metadata, IDs and embedded artifacts;
5. `gameengine_math`: verifies matrices, camera transforms, procedural cube data, UVs, normals and texture/material constants;
6. `gameengine_runtime_smoke`: verifies that the runtime initializes Vulkan, renders a frame and destroys the X11 window;
7. `gameengine_platform_x11`: verifies the platform lifecycle and idempotent shutdown;
8. `gameengine_vulkan_resize`: verifies resource creation/upload/destruction, procedural material descriptors, swapchain recreation, pipeline-cache load/persist/discard behavior and explicit development reload;
9. `gameengine_shader_pipeline`: verifies variant selection and pipeline-cache identity validation;

The Windows-only unit checks additionally cover the native Win32 platform path. The 3D integration test verifies depth resources, indexed drawing, swapchain recreation and validation-clean shutdown.

To build the core and platform without Vulkan, configure with `-DGAMEENGINE_BUILD_VULKAN=OFF`. This uses the renderer stub and does not require Vulkan headers.

Bootstrap shaders are regenerated offline with the explicit CMake target:

```sh
cmake --build build/<preset> --target gameengine_compile_bootstrap_shaders --config Debug
```

The target requires `slangc` version `2026.13.1-1-g84792eb15`. Set `GAMEENGINE_SLANGC` to its executable path when it is not available on `PATH`:

```powershell
$env:GAMEENGINE_SLANGC = 'C:\path\to\slangc.exe'
cmake --build build/windows-msvc-debug --target gameengine_compile_bootstrap_shaders --config Debug
```

The generator writes the checked-in `src/engine/renderer/vulkan/triangle_shaders.hpp` header and caches untracked SPIR-V/reflection artifacts under `build/shader-cache/<Debug|Release>/<shader-id>/`. Each artifact contains a source hash, compiler/target/stage metadata and a deterministic SHA-256 ID. It validates the compiler version, SPIR-V alignment and reflection output. A cache hit does not invoke `slangc`.

The regular runtime build does not require `slangc` because the generated header is already included in the repository. The Vulkan renderer stores a device-specific pipeline cache in `gameengine.pipeline.cache` by default; `RendererConfiguration::pipeline_cache_path` can select another path. The C++ configuration also exposes explicit development-only `reload_shaders()` support. The bootstrap mesh, checkerboard texture and material constants are generated in memory; no asset file is required. The C ABI is unchanged.

### Offline asset tools

The Rust workspace is optional to the C++ runtime and requires only Rust, `serde` and `serde_json`:

```sh
cargo test --manifest-path tools/Cargo.toml
cargo fmt --manifest-path tools/Cargo.toml -- --check
cargo clippy --manifest-path tools/Cargo.toml -- -D warnings
```

The `gameengine-asset-tool` binary supports `validate`, `inspect`, `import-gltf`, `pack-mesh`, `pack-texture`, `pack-material`, `pack-scene` and `package`. The initial compiler accepts synthetic RGBA8 data in memory; PNG/JPG decoding and advanced glTF features remain outside this phase. The tools write `.gemesh`, `.getex`, `.gemat`, `.gescene` and package data through temporary files, while incremental artifacts live under `build/asset-cache/<source-hash>-<tool-version>/`. See [the asset architecture](docs/architecture/assets.md) for the version-1 container contract and runtime boundary.

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
