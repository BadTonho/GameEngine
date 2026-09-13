# GameEngine Roadmap

This document turns the vision in `Idea.md` into an execution sequence. The goal is to build a public, modern and efficient C++ game engine that can evolve for many years.

The roadmap does not define deadlines. Each phase is a technical milestone. A phase may take as long as necessary, be split into several prototypes, or return for revision when tests show that a decision was wrong.

## Current status

The repository is in Phase 0. The initial public foundation, CMake build, minimal C++ runtime and local tests are implemented. The GitHub Actions workflow is prepared but still needs to run in the remote repository.

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
- [ ] Linux/GCC configure, build and CTest pass locally or in CI.
- [ ] Linux/Clang configure, build and CTest pass locally or in CI.
- [ ] Linux AddressSanitizer and UndefinedBehaviorSanitizer pass in CI.
- [ ] The public remote repository has completed its first CI run.

## Phase 1 — Minimal core and platform

Goal: create a small, predictable runtime that shuts down correctly.

### Tasks

- [ ] Define basic types, error conventions and result values.
- [ ] Create logging, assertions and development diagnostics.
- [ ] Define ownership, lifetime and allocation policies.
- [ ] Create allocators only after their use cases are defined.
- [ ] Create opaque handles and generation-based IDs where needed.
- [ ] Create the minimum math types for the first scene.
- [ ] Create the platform layer.
- [ ] Create the Windows window and event loop.
- [ ] Add initial Linux support without spreading conditionals through the core.
- [ ] Create keyboard and mouse input.
- [ ] Create timing and the main loop.
- [ ] Define shutdown and resource-destruction paths.
- [ ] Add memory, handle, timing and event tests.

### Completion criteria

The runtime opens a window, processes events, receives input, runs its loop and closes without leaks or pending resources in Debug and Sanitizer configurations.

## Phase 2 — Vulkan and minimal RHI

Goal: initialize the GPU and establish a small abstraction based on real needs.

### Tasks

- [ ] Create a Vulkan instance.
- [ ] Enable validation layers in development builds.
- [ ] Create a surface, select a GPU and query capabilities.
- [ ] Create the device, queues and command pools.
- [ ] Create the swapchain and basic synchronization.
- [ ] Define the minimal RHI for devices, buffers, images, samplers and pipelines.
- [ ] Create command lists, fences, semaphores and frames in flight.
- [ ] Define deferred GPU resource destruction.
- [ ] Create buffer and texture uploads through staging resources.
- [ ] Add debug names and capture-tool integration.
- [ ] Render a triangle.
- [ ] Validate resize and surface-loss paths where applicable.

### Completion criteria

A C++ application opens a window, initializes Vulkan, renders a triangle, reacts to resize and exits without validation-layer errors.

## Phase 3 — Shader pipeline

Goal: make shaders reproducible, portable and independent of the compiler in the final runtime.

### Tasks

- [ ] Pin the Slang toolchain version used by the project.
- [ ] Define module, entry-point, stage and profile conventions.
- [ ] Compile Slang shaders to SPIR-V in the offline pipeline.
- [ ] Define resource layouts between C++ and the GPU.
- [ ] Generate or validate shader reflection data.
- [ ] Create deterministic identifiers for shaders, targets and variants.
- [ ] Create a shader cache and pipeline cache.
- [ ] Separate Debug and Release artifacts.
- [ ] Produce useful diagnostics for compilation failures.
- [ ] Define how Vulkan capabilities select variants.
- [ ] Keep hot reload restricted to development/editor builds.
- [ ] Ensure exported games do not depend on the Slang compiler.

### Completion criteria

A Slang shader is compiled offline, validated, loaded by the runtime and used for rendering without requiring the compiler in the final executable.

## Phase 4 — First 3D scene

Goal: move beyond the triangle and render a small scene with correct visual foundations.

### Tasks

- [ ] Create a camera and transformations.
- [ ] Create vertex and index buffers.
- [ ] Load a prepared mesh.
- [ ] Load a texture and sampler.
- [ ] Create a basic material.
- [ ] Create a depth buffer and depth testing.
- [ ] Create basic lighting.
- [ ] Implement initial PBR.
- [ ] Implement HDR and tone mapping.
- [ ] Create a small reference scene.
- [ ] Measure CPU, GPU, RAM, VRAM, draw calls and startup.
- [ ] Add a reference capture for visual regression detection.

### Completion criteria

A reference scene loads and displays a textured mesh, camera, material and basic lighting in Vulkan with reproducible visual results and metrics.

## Phase 5 — Asset formats and offline tools

Goal: make the runtime consume prepared, compact and validated data.

### Tasks

- [ ] Define versioned binary formats for meshes, textures, materials and scenes.
- [ ] Validate sizes, offsets, counts, versions and references.
- [ ] Create a Rust workspace for offline tools.
- [ ] Create an initial glTF importer.
- [ ] Create a texture compiler.
- [ ] Create a mesh and vertex-data compiler.
- [ ] Create a material and dependency compiler.
- [ ] Create a packager for the final game format.
- [ ] Create an incremental import cache.
- [ ] Define compression and alignment policies.
- [ ] Produce errors that include the source file and resource.
- [ ] Test malformed assets as untrusted input.
- [ ] Add hot reload only to the development workflow.

### Completion criteria

An original asset is imported offline, converted to the engine format, validated, packaged and loaded by the runtime without interpreting the original heavy format.

## Phase 6 — Scenes, entities and data

Goal: create the scene data foundation without committing to an unmeasured design.

### Tasks

- [ ] Define entities as handles protected against invalid reuse.
- [ ] Implement transforms and hierarchy.
- [ ] Define component storage.
- [ ] Prototype ECS alternatives when needed.
- [ ] Measure iteration, creation, destruction and queries.
- [ ] Define scene serialization.
- [ ] Create camera, mesh, material and light components.
- [ ] Define dependencies between systems.
- [ ] Add validity, generation, hierarchy and serialization tests.
- [ ] Compare RAM and CPU across scenes with different entity counts.

### Completion criteria

A scene can be created, saved, loaded and updated with organized data, valid handles and benchmarks that justify the selected model.

## Phase 7 — Scalable renderer

Goal: build the rendering architecture that supports high quality without imposing every cost on every project.

### Tasks

- [ ] Define real pass dependencies before freezing the render graph.
- [ ] Implement the chosen lighting path based on benchmarks.
- [ ] Implement IBL and shadows.
- [ ] Implement instancing.
- [ ] Implement frustum culling.
- [ ] Evaluate GPU culling and indirect drawing.
- [ ] Evaluate Forward+, Clustered or Deferred for different scene classes.
- [ ] Create quality levels and fallbacks.
- [ ] Add GPU timestamps and per-pass reports.
- [ ] Control VRAM use and temporary-resource lifetime.
- [ ] Verify that advanced resources do not increase the basic-path cost when absent.

### Completion criteria

The renderer supports a larger scene, provides scalable graphics paths and reports measured per-pass costs on reference hardware.

## Phase 8 — Separate editor

Goal: provide a useful public tool without contaminating the exported runtime.

### Tasks

- [ ] Define the project format.
- [ ] Create separate editor and game initialization.
- [ ] Reuse the runtime renderer in the viewport.
- [ ] Create the viewport.
- [ ] Create the hierarchy.
- [ ] Create the inspector.
- [ ] Create the asset browser.
- [ ] Create the console and diagnostics.
- [ ] Create a basic profiler.
- [ ] Create save, load and undo operations when needed.
- [ ] Separate Editor and Runtime modules in the build.
- [ ] Measure editor RAM and startup with an empty project.

### Completion criteria

A user can create a project, open a scene, add objects, edit properties, save and view the result through the engine renderer.

## Phase 9 — Optional runtime modules

Goal: add production features without making every game depend on them.

Each module must have an API, build target, tests, documentation, measured cost and removal path.

### Suggested order

- [ ] Animation.
- [ ] Audio.
- [ ] Physics integration.
- [ ] Optional Lua gameplay scripting.
- [ ] Navigation.
- [ ] Networking.
- [ ] Video and other specific modules when there is a real need.

### Completion criteria

A project can select the required modules in its build and packaging without loading unused optional modules, editor code or importers.

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
