# GameEngine Roadmap

This document turns the vision in `Idea.md` into an execution sequence. The goal is to build a public, modern and efficient C++ game engine that can evolve for many years.

The roadmap does not define deadlines. Each phase is a technical milestone. A phase may take as long as necessary, be split into several prototypes, or return for revision when tests show that a decision was wrong.

## Current status

The repository completed Phase 2 through Phase 6, the Phase 7 renderer foundation and the Phase 8
editor foundation. Phase 9A transform animation is complete and Phase 9B procedural audio is now
implemented as an optional module. Forward+ is the selected production direction, with the
procedural cube, generated lights and generated audio tone remaining reference fixtures. The
final hardware baseline, VRAM policy and later optional runtime modules remain open.

## Evolution rules

- validate important decisions with prototypes and measurements;
- do not freeze abstractions before their real requirements are known;
- keep C++ as the primary runtime language;
- keep the C ABI small and stable;
- keep Rust outside the runtime and limited to offline tools;
- keep Lua optional;
- keep the editor and importers outside the final game;
- give every optional module a clear build boundary;
- do not add an advanced feature without defining its cost, fallback and test strategy;
- advance only when the current milestone is reproducible and debuggable.

## Phase 0 — Public foundation

Goal: allow another person to clone the repository, understand the project and build it.

### Tasks

- [x] Define the project license: MIT.
- [x] Create `README.md` with the vision, current status and build instructions.
- [x] Create `CONTRIBUTING.md`.
- [x] Create `CODE_OF_CONDUCT.md` and contribution rules.
- [x] Define the version and compatibility policy.
- [x] Choose C++20 as the minimum language standard.
- [x] Define the initial compiler and platform matrix.
- [x] Create `CMakeLists.txt` and `CMakePresets.json`.
- [x] Separate Debug, Release and Sanitizer configurations.
- [x] Configure elevated warnings, formatting and static analysis files.
- [x] Create GitHub Actions for Windows and Linux builds and tests.
- [x] Define the dependency, license and third-party code policy.
- [x] Define the initial directory structure without speculative empty modules.

### Completion criteria

A new contributor can clone the project, configure a compiler, build an empty executable and run the tests without undocumented local files or paths.

### Validation status

- [x] Windows/MSVC configure, build and CTest pass locally.
- [x] Linux/GCC configure, build and CTest pass locally.
- [x] Linux/Clang configure, build and CTest pass locally.
- [ ] Linux AddressSanitizer and UndefinedBehaviorSanitizer pass in CI.
- [ ] The public remote repository has completed its first CI run.

Local Linux validation performed with GCC 15.2.0 and Clang 21.1.8:

- Debug and Release builds passed with CTest.
- ASan/UBSan tests passed with leak detection disabled because the current
  execution environment blocks LeakSanitizer through `ptrace`.
- Full sanitizer and remote CI validation remain pending on GitHub Actions.

## Phase 1 — Minimal core and platform

Goal: create a small, predictable runtime that shuts down correctly.

Phase 1 is implemented for Linux with an Xlib backend and Windows with a Win32
backend. The core, platform and compilation checks pass locally on both platforms.

### Tasks

- [x] Define basic types, error conventions and result values.
- [x] Create logging, assertions and development diagnostics.
- [x] Define ownership, lifetime and allocation policies.
- [x] Create allocators only after their use cases are defined.
- [ ] Create opaque handles and generation-based IDs where needed.
- [ ] Create the minimum math types for the first scene.
- [x] Create the platform layer.
- [x] Create the Windows window and event loop.
- [x] Add initial Linux support without spreading conditionals through the core.
- [x] Create keyboard and mouse input.
- [x] Create timing and the main loop.
- [x] Define shutdown and resource-destruction paths.
- [ ] Add memory and handle tests.
- [x] Add timing and event tests.

### Completion criteria

The runtime opens a window, processes events, receives input, runs its loop and closes without leaks or pending resources in Debug and Sanitizer configurations.

## Phase 2 — Vulkan and minimal RHI

Goal: initialize the GPU and establish a small abstraction based on real needs.

Phase 2 is implemented for Linux (Xlib) and Windows (Win32), supporting desktop
swapchains, surface recreation on resize, and a precompiled SPIR-V bootstrap shader path.

### Tasks

- [x] Create a Vulkan instance.
- [x] Enable validation layers in development builds.
- [x] Create a surface, select a GPU and query capabilities.
- [x] Create the device, queues and command pools.
- [x] Create the swapchain and basic synchronization.
- [x] Define the minimal RHI for devices, buffers, images, samplers and pipelines.
- [x] Create command lists, fences, semaphores and frames in flight.
- [x] Define deferred GPU resource destruction.
- [x] Create buffer and texture uploads through staging resources.
- [x] Add debug names and capture-tool integration.
- [x] Render a triangle.
- [x] Validate resize and surface-loss paths where applicable.

### Completion criteria

A C++ application opens a window, initializes Vulkan, renders a triangle, reacts to resize and exits without validation-layer errors.

## Phase 3 — Shader pipeline

Goal: make shaders reproducible, portable and independent of the compiler in the final runtime.

### Tasks

- [x] Pin the Slang toolchain version used by the project.
- [x] Define module, entry-point, stage and profile conventions.
- [x] Compile Slang shaders to SPIR-V in the offline pipeline.
- [x] Define resource layouts between C++ and the GPU.
- [x] Generate or validate shader reflection data.
- [x] Create deterministic identifiers for shaders, targets and variants.
- [x] Create a shader cache and pipeline cache.
- [x] Separate Debug and Release artifacts.
- [x] Produce useful diagnostics for compilation failures.
- [x] Define how Vulkan capabilities select variants.
- [x] Keep hot reload restricted to development/editor builds.
- [x] Ensure exported games do not depend on the Slang compiler.

### Completion criteria

A Slang shader is compiled offline, validated, selected by Vulkan capabilities and used for rendering without requiring the compiler in the final executable. Deterministic artifacts and a device-specific persistent pipeline cache make repeated builds and pipeline creation reproducible.

### Phase 3 validation status

- [x] Bootstrap triangle source migrated from GLSL to Slang.
- [x] SPIR-V and reflection artifacts generated by a cross-platform CMake script.
- [x] Generated SPIR-V header remains usable without Slang at runtime.
- [x] Vulkan pipeline and smoke tests use the named Slang entry points.
- [x] Deterministic shader IDs, Debug/Release shader caches and generated artifact metadata are stable.
- [x] Vulkan capability selection has a mandatory Vulkan 1.0 fallback and rejects unsupported variants before pipeline creation.
- [x] Device-specific Vulkan pipeline cache is validated, loaded, persisted atomically and ignored when incompatible.
- [x] Explicit development-only shader reload replaces pipelines transactionally after successful reconstruction.
- [x] Debug and Release builds, CTest, cache regeneration and Vulkan integration coverage pass locally.

## Phase 4 — First 3D scene

Goal: move beyond the triangle and render a small scene with correct visual foundations.

### Tasks

- [x] Create a camera and transformations.
- [x] Create vertex and index buffers.
- [ ] Load a prepared mesh.
- [x] Generate a procedural texture and sampler.
- [x] Create a procedural material.
- [x] Create a depth buffer and depth testing.
- [x] Create basic directional lighting.
- [x] Implement initial PBR.
- [x] Implement HDR and tone mapping.
- [x] Create a small procedural reference scene.
- [ ] Measure CPU, GPU, RAM, VRAM, draw calls and startup.
- [ ] Add a reference capture for visual regression detection.

### Phase 4 vertical slice validation

- [x] Implement custom `Vec3`/`Mat4` math with right-handed Vulkan `0..1` depth projection.
- [x] Render a deterministic procedural indexed cube with a static camera.
- [x] Use push constants for the 128-byte model and view-projection matrices.
- [x] Bind a procedural checkerboard texture and internal PBR material through Vulkan descriptors.
- [x] Apply directional lighting and deterministic HDR tone mapping in Slang.
- [x] Validate Debug and Release builds, shader regeneration and CTest locally.
- [ ] Add prepared mesh loading, performance metrics and reference capture systems.

### Completion criteria

A reference scene loads and displays a textured mesh, camera, material and basic lighting in Vulkan with reproducible visual results and metrics.

## Phase 5 — Asset formats and offline tools

Goal: make the runtime consume prepared, compact and validated data.

### Tasks

- [x] Define versioned binary formats for meshes, textures, materials and scenes.
- [x] Validate sizes, offsets, counts, versions, flags and references.
- [x] Create a Rust workspace for offline tools.
- [x] Create an initial JSON glTF importer with a synthetic fixture.
- [x] Create an RGBA8-in-memory texture compiler.
- [x] Create a mesh and vertex-data compiler.
- [x] Create a material and dependency compiler.
- [x] Create a deterministic packager for the initial package container.
- [x] Create an incremental import cache keyed by source, options and tool version.
- [x] Define 16-byte alignment and no-compression version-1 policies.
- [x] Produce contextual errors for unsupported source features and malformed data.
- [x] Test malformed assets as untrusted input in Rust and C++.
- [ ] Add hot reload only to the development workflow.

### Completion criteria

The version-1 formats, validators, Rust workspace, synthetic fixtures and C++ zero-copy reader are complete. The renderer still uses its procedural bootstrap scene; replacing it with prepared assets, image decoding, broader glTF coverage and runtime asset streaming remain later work.

## Phase 6 — Scenes, entities and data

Goal: create the scene data foundation without committing to an unmeasured design.

### Tasks

- [x] Define entities as handles protected against invalid reuse.
- [x] Implement transforms and hierarchy.
- [x] Define component storage.
- [x] Prototype ECS alternatives when needed.
- [x] Measure iteration, creation, destruction and queries.
- [x] Define scene serialization.
- [x] Create camera, mesh, material and light components.
- [x] Define dependencies between systems.
- [x] Add validity, generation, hierarchy and serialization tests.
- [x] Compare RAM and CPU across scenes with different entity counts.

### Completion criteria

A scene can be created, saved, loaded and updated with organized data, valid handles and optional synthetic benchmarks for the initial sparse-set model. Real asset loading and a final ECS storage choice remain future work.

## Phase 7 — Scalable renderer

Goal: build the rendering architecture that supports high quality without imposing every cost on every project.

### Tasks

- [x] Define real pass dependencies before freezing the render graph.
- [x] Select Forward+ as the production lighting path and keep Clustered/Deferred benchmark-only.
- [x] Implement procedural IBL/environment and a directional shadow map for the High profile.
- [x] Implement procedural instancing with deterministic 1k/10k/100k workloads.
- [x] Implement CPU frustum culling with a conservative invalid-frustum fallback.
- [x] Evaluate GPU culling and indirect drawing.
- [x] Evaluate Forward+, Clustered and Deferred through the deterministic benchmark protocol.
- [x] Create Low/Medium/High quality selection with an explicit Low/Medium fallback policy.
- [x] Add optional GPU timestamps and per-pass reports with a CPU fallback.
- [ ] Control VRAM use and temporary-resource lifetime.
- [ ] Verify that advanced resources do not increase the basic-path cost when absent.

### Phase 7A validation status

- [x] Create an internal deterministic graph with imported color/depth resources and `forward_opaque`.
- [x] Validate pass dependencies, resource access, duplicate writes and cycles.
- [x] Execute the graph through the existing Vulkan render pass without changing the public RHI.
- [x] Add CPU pass timing and optional Vulkan timestamp queries.
- [x] Add the development-only `gameengine_runtime --metrics` report.
- [x] Validate Debug build, CTest, Vulkan smoke/resize and the metrics workload locally.

### Phase 7B validation status

- [x] Render the procedural cube through two Vulkan vertex bindings with instance-rate model data.
- [x] Keep persistent host-visible instance storage partitioned by frame in flight.
- [x] Generate centered 3D workloads of 1,000, 10,000 and 100,000 instances deterministically.
- [x] Measure CPU visibility, visible/culled counts, draw calls and optional GPU pass timing.
- [x] Validate instance layout, frustum extraction, fallback visibility and validation-clean Vulkan
  smoke/resize/metrics execution.

### Phase 7C validation status

- [x] Add opt-in Vulkan compute culling with a Vulkan 1.0-compatible atomic append path.
- [x] Add persistent source, visible and indirect buffers with per-frame output slices.
- [x] Connect `gpu_cull` to `forward_opaque` through the internal render graph and synchronize
  compute writes with vertex/indirect reads.
- [x] Add GPU/CPU visibility metrics, `--gpu-culling`, indirect drawing and CPU fallback.
- [x] Validate Debug smoke, GPU metrics and validation-clean shutdown locally.

The GPU visible-instance order is not a contract because the current atomic compaction is intended
for opaque depth-tested geometry. Forward+ is now the productive lighting path, with shadows,
procedural environment and quality fallbacks implemented; VRAM policy and the final performance
comparison between Forward+, Clustered and Deferred remain open Phase 7 work.

### Phase 7D validation status

- [x] Add a deterministic internal `--renderer-benchmark` protocol with 36 fixed workload cases.
- [x] Generate procedural point lights and report Forward, Forward+, Clustered and Deferred
  prototype pass structure without requiring external models or textures.
- [x] Add offline Slang compute artifacts for the benchmark and keep the generated header as the
  runtime fallback.
- [x] Report startup, CPU/GPU pass timing, draws, dispatches and available RAM/VRAM/resource
  metrics without versioning numeric benchmark results.
- [ ] Choose the production lighting architecture from reference-hardware measurements.
- [x] Implement production-quality procedural Forward+ with Low/Medium/High fallbacks, shadows and environment.

The benchmark is intentionally isolated from the normal renderer. Unsupported Vulkan builds print
`unavailable` and exit successfully; no automatic architecture decision is made from synthetic
workloads.

### Phase 7E validation status

- [x] Add internal `--renderer-quality low|medium|high` parsing with Medium as the default.
- [x] Keep Low as the directional-light fallback and add deterministic 16x16 Forward+ tile-list
  preparation for Medium using procedural lights only.
- [x] Extend the offline Slang artifact table with Forward+, shadow and environment entry points,
  reflection validation and deterministic IDs.
- [x] Add unit coverage for quality resolution, tile lists, shadow matrices and procedural
  cubemap mip data.
- [x] Complete validated Vulkan shadow-map and cubemap sampling for the High profile on the local reference device.
- [ ] Document the local reference-hardware baseline and confirm advanced-resource lifetime/VRAM
  policy before closing Phase 7.

### Completion criteria

The renderer supports a larger scene, provides scalable graphics paths and reports measured per-pass costs on reference hardware.

## Phase 8 — Separate editor

Goal: provide a useful public tool without contaminating the exported runtime.

### Tasks

- [x] Define the `.geproject` v1 project format and deterministic procedural project creation.
- [x] Create separate editor and game initialization.
- [x] Reuse the runtime renderer in the viewport through an internal scene bridge.
- [x] Create the procedural Vulkan viewport with an editor-only UI overlay.
- [x] Create the deterministic entity hierarchy view and selection hit testing.
- [x] Create the basic transform inspector and keyboard editing path.
- [x] Create the read-only project asset browser with prepared-asset validation.
- [x] Create the bounded editor console and diagnostics panel.
- [x] Create a basic render/session profiler with unavailable-metric fallbacks.
- [x] Create deterministic save/load for the project and scene.
- [x] Separate Editor and Runtime modules in the build.
- [x] Measure editor RAM and startup with an empty project when platform APIs provide it.

### Completion criteria

A user can create a procedural project, open its scene, select the bootstrap entity, edit its
transform, save and view the result through the engine renderer. Adding arbitrary objects and
assets remains later Phase 8 work.

### Phase 8B validation status

- [x] Add a deterministic, project-root-only catalog for `.gemesh`, `.getex`, `.gemat` and
  `.gescene`, using the existing C++ validators and no asset ownership in the UI.
- [x] Add a fixed-capacity read-only console that mirrors messages to `stderr`.
- [x] Add a profiler panel backed by renderer pass reports, CPU/GPU fallback state and sampled
  editor startup/process-memory metrics.
- [x] Add explicit refresh, asset selection, panel switching, malformed-asset diagnostics and
  no-Vulkan `unavailable` behavior.
- [x] Validate Debug, no-Vulkan editor builds, editor unit tests and Vulkan editor smoke tests
  locally.

Asset editing/import, previews, file dialogs, undo/redo and runtime prepared-asset integration
remain later Phase 8 work.

## Phase 9 — Optional runtime modules

Goal: add production features without making every game depend on them.

Each module must have an API, build target, tests, documentation, measured cost and removal path.

### Suggested order

- [x] Animation transform/keyframe foundation with an optional C++ target, deterministic procedural
  clip, runtime/editor integration, fixed-step tests and measured synthetic workloads.
- [x] Procedural audio foundation with fixed PCM mixing, native backends, stub fallback and diagnostics.
- [ ] Physics integration.
- [ ] Optional Lua gameplay scripting.
- [ ] Navigation.
- [ ] Networking.
- [ ] Video and other specific modules when there is a real need.

### Completion criteria

A project can select the required modules in its build and packaging without loading unused optional modules, editor code or importers.

### Phase 9A validation status

- [x] Add `gameengine_animation` behind `GAMEENGINE_BUILD_ANIMATION` without changing the RHI or
  C ABI.
- [x] Add validated TRS clips, shortest-path quaternion interpolation, looping and fixed-step
  updates without per-frame allocations.
- [x] Integrate the procedural clip into runtime and editor, pausing it before transform edits.
- [x] Add unit tests, optional 1k/10k/100k benchmarks and documentation with an explicit removal
  path.

Physics, Lua, navigation, networking, video, skeletons, skinning and serialized animation assets
remain later Phase 9 modules.

### Phase 9B validation status

- [x] Add `gameengine_audio` behind `GAMEENGINE_BUILD_AUDIO` without changing the RHI, C ABI or
  asset formats.
- [x] Add deterministic 48 kHz stereo procedural sine mixing with fixed voice storage, generation
  handles, command queue and no allocation in the callback path.
- [x] Add WASAPI and optional ALSA backends with a stub fallback and explicit unavailable reporting.
- [x] Add runtime/editor audio smoke diagnostics, unit tests, an optional 1/16/64 voice benchmark
  and architecture documentation.
- [ ] Complete Linux native-backend validation on a reference ALSA environment and sanitizer
  validation for the optional audio thread.

Audio files, codecs, streaming, 3D spatialization, DSP effects, music graphs and prepared audio
assets remain future work.

## Phase 10 — Continuous quality, security and performance

Goal: prevent engine growth from destroying its original principles.

### Tasks

- [ ] Create versioned benchmark scenes and workloads.
- [ ] Measure startup, RAM, VRAM, CPU, GPU, draw calls, loading and executable size.
- [ ] Create a baseline and regression detection.
- [ ] Run Sanitizers and static analysis in CI.
- [ ] Repeatedly test shutdown and destruction.
- [ ] Fuzz parsers and asset formats.
- [ ] Test handles, ownership and references after destruction.
- [ ] Integrate validation layers and debug markers.
- [ ] Document platform and GPU limitations.
- [ ] Create Low, Medium and High profiles with explicit fallbacks.
- [ ] Measure before and after important optimizations.

### Completion criteria

Every meaningful change has evidence of correctness, cost and impact. Performance and memory regressions are detected before a release.

## Phase 11 — Additional platforms and backends

Goal: expand the engine without weakening the initial Vulkan backend.

### Tasks

- [ ] Stabilize the RHI contract from real Vulkan use.
- [ ] Define a capabilities matrix for each backend.
- [ ] Implement D3D12 after the common renderer is stable.
- [ ] Validate Slang shaders and layouts in DXIL.
- [ ] Implement Metal support when its toolchain and capabilities path is mature.
- [ ] Validate the Metal target and keep platform-specific exceptions isolated.
- [ ] Add each platform to CI and its corresponding benchmarks.

### Completion criteria

The same project can use more than one backend without scene, asset, gameplay or public API code knowing graphics-API details.

## Phase 12 — Advanced graphics features

Goal: add advanced visual quality only after the foundation is stable and costs can be controlled.

### Evaluation order

- [ ] TAA and temporal reconstruction.
- [ ] Upscaling.
- [ ] SSAO.
- [ ] SSR.
- [ ] Contact shadows.
- [ ] Volumetric fog.
- [ ] GPU particles.
- [ ] Real-time GI.
- [ ] Ray tracing.
- [ ] Virtualized geometry.

Each feature must include:

- [ ] CPU/GPU/RAM/VRAM cost;
- [ ] startup and build-size impact;
- [ ] a fallback for weaker hardware;
- [ ] visual and stability tests;
- [ ] a way to remove it from the build when unused.

## Phase 13 — Public ecosystem and releases

Goal: turn the engine into a reliable project for external users and contributors.

### Tasks

- [ ] Publish installation, architecture and usage documentation.
- [ ] Publish small, complete examples.
- [ ] Publish project templates.
- [ ] Document the C ABI and C++ SDK.
- [ ] Create compatibility and deprecation policies.
- [ ] Create a changelog and release notes.
- [ ] Distribute binaries and tools reproducibly.
- [ ] Define pull-request review rules.
- [ ] Create beginner issues and contribution areas.
- [ ] Document plugin creation.
- [ ] Create public API compatibility tests.
- [ ] Publish benchmarks with methodology, not numbers alone.

### Completion criteria

An external person can install the engine, follow an example, create a project, understand the architecture, contribute code and update to a new version using documented rules.

## First concrete milestone

The first implementation goal is the remaining validation of Phase 0:

```text
clean clone
  ↓
CMake configure
  ↓
C++ compile
  ↓
empty runtime
  ↓
CTest
  ↓
sanitizers and CI
```

After that, the next visual milestone is:

```text
window
  ↓
Vulkan
  ↓
Slang offline
  ↓
triangle
  ↓
mesh + texture + camera
  ↓
PBR scene
```

This path creates a real foundation for evaluating later decisions without abandoning the complete engine vision.
