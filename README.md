# GameEngine

[![CI](https://github.com/BadTonho/GameEngine/actions/workflows/ci.yml/badge.svg)](https://github.com/BadTonho/GameEngine/actions/workflows/ci.yml)

A public, modular C++ game engine focused on high visual quality, efficient hardware usage and long-term maintainability.

> **Status: early development — Phase 7E Forward+ consolidation in progress.** The repository provides a Vulkan rendering foundation with an offline Slang shader pipeline, a procedural instanced cube, CPU frustum culling, opt-in GPU culling/indirect drawing, production Low/Medium/High Forward+ profiles and measured render passes. It is not ready to create a complete game yet.

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

Phase 0 through Phase 6 are implemented in the current procedural scope. Phase 7E promotes the Low/Medium/High Forward+ profiles while keeping Clustered and Deferred benchmark-only:

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
- internal render graph with deterministic pass dependencies and the `forward_opaque` pass;
- optional Vulkan timestamp queries with CPU fallback and per-pass timing reports;
- development metrics mode through `gameengine_runtime --metrics`, covering 1k, 10k and 100k procedural instances;
- internal `Vec3`/`Mat4` math with right-handed Vulkan-compatible perspective and look-at transforms;
- deterministic procedural instancing with a persistent per-frame host-visible instance buffer and CPU frustum culling;
- opt-in Vulkan compute culling with atomic compaction, persistent GPU buffers and indexed indirect drawing, with CPU fallback;
- an internal `--renderer-benchmark` mode comparing procedural Forward, Forward+, Clustered and Deferred prototypes over deterministic 1k/10k/100k instance and 1/32/256 light workloads;
- internal quality selection through `--renderer-quality low|medium|high`, with Low fallback when compute/storage support is unavailable;
- deterministic 16x16 Forward+ tile-list preparation, procedural point-light data, a 1024² directional shadow map, a mipmapped 64² procedural cubemap and versioned Forward+/shadow/environment Slang artifacts;
- versioned benchmark reports in `build/renderer-benchmarks/lighting_benchmark_v1.txt`, with explicit unavailable markers for unsupported RAM/VRAM metrics;
- generation-checked handles and fence-based deferred resource destruction;
- Vulkan object names and command labels through `VK_EXT_debug_utils`;
- `gameengine_tests`, `gameengine_asset_tests`, `gameengine_rhi_tests`, `gameengine_math_tests`, `gameengine_scene_tests`, render graph, renderer metrics, platform and shader pipeline tests without an external test framework;
- CTest integration with core, assets, RHI, math, scene, render graph, metrics, runtime, Vulkan, shader pipeline and X11 smoke tests;
- high-warning builds with warnings treated as errors;
- Debug, Release and GCC sanitizer presets;
- GitHub Actions for Windows/MSVC, Linux/GCC, Linux/Clang and ASan/UBSan.

The following are intentionally outside the current procedural renderer scope:

- prepared mesh/assets loading;
- asset-file texture/material loading;
- runtime shader file loading, automatic polling and editor integration;
- prepared asset runtime integration, editor, Lua and a final ECS storage choice;
- VRAM policy and the hardware baseline for the final lighting decision;
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

CTest covers the core, asset, RHI, math, scene, render graph, metrics, procedural-instancing, runtime, Vulkan, shader pipeline and platform paths:

1. `gameengine_core`: verifies the core initialization and shutdown lifecycle;
2. `gameengine_assets`: validates synthetic versioned asset containers and zero-copy views;
3. `gameengine_rhi`: verifies RHI lifecycle error handling and Vulkan utility policies;
4. `gameengine_shaders`: verifies SPIR-V layout, metadata, IDs and embedded artifacts;
5. `gameengine_math`: verifies matrices, camera transforms, procedural cube data, UVs, normals and texture/material constants;
6. `gameengine_runtime_smoke`: verifies that the runtime initializes Vulkan, renders a frame and destroys the X11 window;
7. `gameengine_platform_x11`: verifies the platform lifecycle and idempotent shutdown;
8. `gameengine_vulkan_resize`: verifies resource creation/upload/destruction, procedural material descriptors, swapchain recreation, pipeline-cache load/persist/discard behavior and explicit development reload;
9. `gameengine_renderer_instances`: verifies deterministic 1k/100k workloads, frustum extraction and CPU visibility ordering;
10. `gameengine_renderer_gpu_culling`: verifies GPU-facing layouts, dispatch sizing and indirect command initialization;
11. `gameengine_runtime_metrics`: verifies the three CPU procedural metrics workloads and their Vulkan lifecycle;
12. `gameengine_runtime_gpu_metrics`: verifies the opt-in GPU culling metrics workloads and fallback lifecycle;
13. `gameengine_shader_pipeline`: verifies variant selection and pipeline-cache identity validation;

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

`gameengine_runtime --metrics` executes 10 warmup frames and 30 measured frames for each procedural workload of 1,000, 10,000 and 100,000 instances. Each block reports CPU visibility time, visible/culled instances, draw calls and optional GPU timing for `forward_opaque`. Add `--gpu-culling` to select the opt-in compute path; its output includes the `gpu_cull` pass, dispatch preparation time, persistent buffer sizes and CPU fallback state when the device has no compute support.

The production lighting profile is selected internally with `--renderer-quality low|medium|high`
and defaults to Medium. Low is the directional-light fallback; Medium builds deterministic 16x16
Forward+ tile data from 256 procedural point lights. High adds the persistent directional shadow
pass and a procedural mipmapped cubemap, with an explicit fallback when the device cannot provide
the required resources. These profiles
are compatible with `--smoke-test`, `--metrics`, `--renderer-benchmark` and `--gpu-culling`.

```text
gameengine_runtime --smoke-test --renderer-quality low
gameengine_runtime --smoke-test --renderer-quality medium
gameengine_runtime --smoke-test --renderer-quality high
gameengine_runtime --metrics --renderer-quality medium
```

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
