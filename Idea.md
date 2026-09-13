# GAME ENGINE — INITIAL VISION

## Objective

Create a modern, extremely lightweight, fast, modular and visually advanced game engine.

The goal is not to be lightweight by having few features. The goal is to deliver **AAA-level visual quality at the lowest practical cost in RAM, CPU, GPU, storage and startup time**.

The engine should scale from modest hardware to high-end machines without forcing every project to load expensive systems.

The engine's main priorities are:

* low RAM usage;
* low idle CPU usage;
* extremely fast startup;
* small executables;
* modern architecture;
* a modern and scalable renderer;
* high visual quality;
* support for modern hardware;
* the ability to run on weak PCs;
* a lightweight and responsive editor;
* fully optional systems;
* cost proportional to the resources actually used;
* explicit memory control;
* high performance;
* ease of use for game developers;
* heavy tools kept outside the runtime whenever possible.

The two central philosophies are:

> You only pay for what you use.

> Maximum visual quality per unit of hardware.

If a game does not use physics, Lua, networking, ray tracing, volumetrics, dynamic GI, particles or another optional system, that code and its data should ideally not exist in the final executable. When complete removal is not technically possible, the residual cost must be small, explicit and measurable.

Graphics quality must be **scalable**, not unconditionally heavy.

## Scope and success criteria

This document describes a long-term vision. It is not a launch deadline and does not require every system to be implemented at the same time.

The initial validation focus is a real-time 3D game engine for desktop, starting with Windows and Linux, with scalable rendering paths. Other platforms, genres and features can be added as the architecture and tests justify them.

In this vision, “best possible” means maximizing the combination of:

* visual quality;
* predictable performance;
* RAM, VRAM, CPU and GPU efficiency;
* stability and correctness;
* ability to evolve;
* simplicity for engine users.

These goals can conflict. Every important decision should state its trade-offs and be validated with prototypes, tests and measurements. The vision may remain ambitious even when an implementation is revised.

# LANGUAGES

## C++

C++ is the primary language of the engine and runtime.

It is responsible for:

* Core
* Memory Management
* Platform Layer
* Window
* Input
* Filesystem
* Threading
* Job System
* Renderer
* RHI
* Render Graph
* Scene System
* ECS
* Asset Runtime
* Animation Runtime
* Audio Runtime
* Physics Integration
* Networking Runtime in the future
* Game Runtime
* Internal Plugins
* Performance-critical code

Rule:

> Everything that runs inside the game should preferably be written in C++.

Objectives:

* no mandatory garbage collector in the core;
* no mandatory heavy runtime;
* explicit ownership and lifetime;
* few allocations;
* small executables;
* direct access to system APIs;
* predictable performance;
* easy integration with C and native APIs;
* data-oriented design whenever it improves the system.

The C++ standard, supported compilers and toolchain configuration must be **pinned** in the repository or build configuration. Updates must be deliberate and tested, never automatic.

C++ does not dictate an object-oriented architecture. The runtime should use data-oriented structures, RAII for scope-bound resources and explicit ownership where those choices improve correctness and performance.

# C ABI

C is used primarily as the engine's public ABI.

It is not necessarily used to implement large subsystems.

Objective:

Create a stable interface between the engine and other languages.

Example:

```c
typedef unsigned long long EngineEntity;

EngineEntity engine_entity_create(void);

void engine_entity_destroy(
    EngineEntity entity
);

void engine_transform_set_position(
    EngineEntity entity,
    float x,
    float y,
    float z
);
```

Internally, these functions may be implemented in C++.

The C ABI may serve as a future integration boundary for:

* C++;
* C;
* Rust;
* C#;
* Lua;
* Python;
* Zig as a future binding, not as the official runtime language;
* other languages.

The C ABI alone does not create bindings automatically. Each language that uses the engine needs its own binding, generator or adapter. The public ABI must remain small, stable and based on simple types.

## Public ABI rules

* use opaque handles instead of exposing internal structures;
* define who allocates, owns and releases each resource;
* version the ABI and validate compatibility;
* represent failures with error codes and documented contracts;
* avoid containers, strings, classes, templates, exceptions and internal C++ layouts at the public boundary;
* define conventions for callbacks, threads and shutdown.

Architecture:

```text
              ENGINE
                 │
                 │
              C ABI
                 │
       ┌─────────┼─────────┐
       │         │         │
      C++        C        Rust
       │         │         │
     Plugins   Plugins   Plugins
```

---

# RUST

Rust is used primarily for external tools.

These tools do not need to be included in the final game.

Possible tools:

```text
Tools/
├── AssetCompiler/
├── TextureCompiler/
├── ShaderTooling/
├── ModelImporter/
├── AnimationCompiler/
├── AssetDatabase/
├── Packager/
├── BuildServer/
└── ProjectTools/
```

Example pipeline:

```text
player.glb
player.png
player_normal.png
walk.fbx

        ↓

Rust Asset / Tool Pipeline

        ↓

Import
Optimization
Compression
Conversion
LOD
Metadata
Processing

        ↓

player.mesh
player.texture
player_normal.texture
walk.animation
```

The runtime should not need to interpret heavy formats during the game.

The goal is to load files already prepared for the engine.

This should improve:

* startup;
* loading;
* RAM;
* CPU;
* runtime size;
* engine simplicity.

---

# LUA

Lua is optional.

It is used primarily for gameplay and scripting.

Example:

```lua
function start()
    player = Engine.findEntity("Player")
end

function update(dt)
    if Input.keyDown("W") then
        player:move(0, 0, 5 * dt)
    end
end
```

Lua is never mandatory.

Example:

```text
Project A

C++
+
Engine
```

Result:

```text
Lua runtime:
0 bytes
```

Another project:

```text
Project B

C++
+
Engine
+
Lua
```

Only in this case does Lua enter the executable.

---

# SLANG

Slang is the engine's default shader language.

It is responsible for:

* vertex shaders;
* fragment/pixel shaders;
* compute shaders;
* ray-tracing shaders in the future;
* shared GPU-code libraries;
* shader specialization and variant generation.

Primary objective:

```text
                 Slang
                   │
        ┌──────────┼──────────┐
        │          │          │
        ↓          ↓          ↓
      SPIR-V      DXIL       Metal
        │          │          │
        ↓          ↓          ↓
     Vulkan       D3D12      Metal
```

The engine should avoid maintaining completely separate versions of the same shader for Vulkan, Direct3D 12 and Metal whenever Slang can provide a shared foundation.

## Shader compilation rule

Shaders should be compiled **offline** whenever possible.

Preferred pipeline:

```text
Shader .slang
    ↓
Shader Compiler / Toolchain
    ↓
SPIR-V / DXIL / Metal output
    ↓
Shader Cache / Pipeline Cache
    ↓
Runtime
```

The Slang compiler and shader compilation tools **must not be mandatory dependencies of the final game**.

In release builds, the runtime should consume shaders already compiled and prepared for the target platform.

Hot reload and development compilation may exist in the editor, but should remain outside the final runtime whenever possible.

# GENERAL ARCHITECTURE

```text
                              GAME ENGINE
                                  │
                                  │
                               C++ CORE
                                  │
          ┌───────────────────────┼───────────────────────┐
          │                       │                       │
        CORE                   RENDERER                 SCENE
          │                       │                       │
      Memory                     RHI                     ECS
      Filesystem              Render Graph             Transform
      Jobs                    GPU Culling              Hierarchy
      Threading               GPU Driven              Components
      Logging                 PBR                     Assets
      Platform                Lighting                Animation
      Input                   Shadows                 Physics
      Timing                  Post FX                 Audio
          │                       │                       │
          └───────────────────────┼───────────────────────┘
                                  │
                                C ABI
                                  │
                       ┌──────────┼──────────┐
                       │          │          │
                      C++         C        Rust
                    Plugins    Plugins    Plugins


                         GPU SHADER LAYER
                                  │
                                Slang
                                  │
                  ┌───────────────┼───────────────┐
                  │               │               │
                SPIR-V           DXIL          Metal target
                  │               │               │
                Vulkan           D3D12           Metal


                        TOOLCHAIN / EDITOR
                                  │
                            C++ + Rust
                                  │
             ┌────────────────────┼────────────────────┐
             │                    │                    │
           Assets               Shaders              Build
             │                    │                    │
         Compiler           Slang Compiler          Packager
         Importer           Cache/Variants          Exporter


                             GAMEPLAY
                                  │
                           C++ or Lua
                                  │
                          Lua OPTIONAL
```

Fundamental separation:

```text
Editor / Toolchain ≠ Final Runtime
```

The exported game should contain only the modules and data necessary for execution.

# PROJECT STRUCTURE

```text
Engine/
│
├── engine/
│   │
│   ├── core/
│   │   ├── memory/
│   │   ├── allocator/
│   │   ├── logging/
│   │   ├── threading/
│   │   ├── jobs/
│   │   ├── timing/
│   │   └── containers/
│   │
│   ├── platform/
│   │   ├── windows/
│   │   ├── linux/
│   │   └── macos/
│   │
│   ├── window/
│   ├── input/
│   ├── filesystem/
│   │
│   ├── renderer/
│   │   ├── rhi/
│   │   ├── vulkan/
│   │   ├── d3d12/
│   │   ├── metal/
│   │   ├── render_graph/
│   │   ├── gpu_driven/
│   │   ├── visibility/
│   │   ├── shaders/
│   │   ├── materials/
│   │   ├── lighting/
│   │   ├── shadows/
│   │   ├── pbr/
│   │   ├── postfx/
│   │   ├── upscaling/
│   │   ├── volumetrics/
│   │   ├── raytracing/
│   │   └── gpu/
│   │
│   ├── scene/
│   │   ├── entity/
│   │   ├── components/
│   │   ├── transform/
│   │   └── hierarchy/
│   │
│   ├── ecs/
│   ├── assets/
│   ├── animation/
│   ├── audio/
│   ├── physics/
│   ├── scripting/
│   │   └── lua/
│   ├── networking/
│   └── runtime/
│
├── api/
│   ├── c/
│   └── cpp/
│
├── shaders/
│   ├── common/
│   ├── materials/
│   ├── lighting/
│   ├── shadows/
│   ├── postfx/
│   ├── compute/
│   └── raytracing/
│
├── editor/
│   ├── core/
│   ├── viewport/
│   ├── hierarchy/
│   ├── inspector/
│   ├── asset_browser/
│   ├── console/
│   ├── profiler/
│   └── project_manager/
│
├── tools/
│   ├── asset_compiler/
│   ├── shader_compiler/
│   ├── shader_cache/
│   ├── texture_compiler/
│   ├── model_importer/
│   ├── animation_compiler/
│   ├── asset_database/
│   ├── packager/
│   └── build_tools/
│
├── plugins/
├── examples/
├── benchmarks/
├── tests/
├── third_party/
├── docs/
├── CMakeLists.txt
└── README.md
```

# MEMORY SYSTEM

The engine should avoid constant malloc/free activity during gameplay.

Objective:

```text
Allocations per frame:

0 or close to 0
```

Initial structure:

```text
Game Memory
│
├── Permanent Arena
│
├── Engine Arena
│
├── Scene Arena
│
├── Asset Arena
│
├── Frame Arena A
│
└── Frame Arena B
```

Frame Arena:

```text
Frame begins

████████████████████░░░░░░

sequential allocations

██████████████████████████

Frame ends

reset()

░░░░░░░░░░░░░░░░░░░░░░░░░░
```

Avoid thousands of malloc/free calls during every frame.

---

# DATA-ORIENTED DESIGN

Avoid extremely object-oriented architectures.

Avoid:

```text
GameObject
├── Transform
├── Physics
├── Renderer
├── Audio
├── Script
└── ...
```

Prefer organized data:

```text
Transforms

[T][T][T][T][T][T][T][T]

Velocities

[V][V][V][V][V][V]

Renderables

[R][R][R][R][R]

Lights

[L][L][L]

Physics Bodies

[P][P][P][P][P]
```

Objectives:

* better CPU cache use;
* better SIMD;
* batch processing;
* better multithreading;
* efficient GPU uploads;
* lower overhead per entity.

---

# ENTITY SYSTEM

Entities should preferably be IDs.

Example:

```text
Entity

64 bits
```

Possible organization:

```text
Entity ID

┌────────────────┬────────────────┐
│ Generation     │ Index          │
└────────────────┴────────────────┘
```

This allows invalid handles and destroyed entities to be detected.

---

# RENDERER

The renderer is one of the engine's central systems.

The goal is **modern/AAA visual quality without making that quality a mandatory cost for every project**.

The renderer must be completely decoupled from the graphics API.

Do not spread Vulkan, Direct3D 12 or Metal through the engine code.

Avoid:

```text
Scene
  ↓
vkCmdDraw()
```

Prefer:

```text
Scene
  ↓
Renderer API
  ↓
Render Graph
  ↓
RHI
  ↓
Backend
```

Structure:

```text
Renderer API
      │
      ↓
Render Graph
      │
      ↓
RHI
      │
 ┌────┼─────┐
 │    │     │
 ↓    ↓     ↓
VK   DX12  Metal
```

Visual quality should scale through levels and modules.

Conceptual example:

```text
BASE
├── PBR
├── HDR
├── Image Based Lighting
├── Shadow Maps
├── Forward+ / Clustered Lighting
├── GPU Culling
└── Instancing

ADVANCED
├── TAA
├── Upscaling
├── SSAO
├── SSR
├── Contact Shadows
├── Volumetric Fog
└── Improved Shadowing

ULTRA / OPTIONAL
├── Real-time Global Illumination
├── Ray Traced Reflections
├── Ray Traced Shadows
├── Virtualized Geometry
├── High-end Volumetrics
└── Advanced Reconstruction/Upscaling
```

Advanced systems must not contaminate the cost of the basic renderer when disabled.

# RHI

RHI means:

```text
Render Hardware Interface
```

It is responsible for abstracting graphics APIs.

Possible objects:

```text
GPUDevice
GPUBuffer
GPUTexture
GPUShader
GPUPipeline
GPUCommandList
GPUFence
GPUSemaphore
GPUSwapchain
```

The engine uses these objects.

The backend converts them to:

```text
Vulkan
Direct3D 12
Metal
```

The RHI should represent concepts genuinely shared between backends. During the first prototypes, it should be small and evolve from needs observed in the Vulkan backend. There is no need to freeze a complete abstraction before a functional renderer exists.

Details specific to each API, optional capabilities and hardware limitations must remain isolated in the backend or be exposed as explicit RHI capabilities.

---

# GRAPHICS BACKENDS

Priority:

```text
1. Vulkan
2. Direct3D 12
3. Metal
```

Initially:

```text
Vulkan
```

Later:

```text
Windows
├── Vulkan
└── Direct3D 12

Linux
└── Vulkan

macOS
└── Metal
```

Shaders:

```text
Slang
  │
  ├── SPIR-V → Vulkan
  ├── DXIL   → Direct3D 12
  └── Metal target → Metal
```

The graphics backend and final shader format must be able to change without requiring scene, material and gameplay systems to know API-specific details.

# MODERN RENDERING TECHNOLOGIES

The engine may evolve to support:

* Vulkan;
* Direct3D 12;
* Metal;
* Slang for shaders;
* GPU-driven rendering;
* bindless resources;
* indirect drawing;
* multi-draw indirect;
* compute shaders;
* async compute;
* render graph;
* GPU frustum culling;
* GPU occlusion culling;
* Hi-Z / hierarchical depth when useful;
* PBR;
* image-based lighting;
* Forward+;
* clustered lighting;
* HDR;
* tone mapping;
* temporal anti-aliasing;
* temporal reconstruction;
* upscaling;
* instancing;
* GPU particles;
* screen-space reflections;
* ambient occlusion;
* volumetric fog;
* contact shadows;
* real-time GI in the future;
* optional ray tracing;
* virtualized geometry in the future;
* texture streaming;
* mesh streaming;
* asynchronous asset streaming.

These technologies should be added progressively.

Modern technology does not mean putting everything into the engine.

Every feature must justify its cost in:

* CPU;
* GPU;
* VRAM;
* RAM;
* executable size;
* complexity;
* build time;
* maintenance.

## Visual principle

The main question should not be:

> How do we add the largest number of effects?

The question should be:

> Which technique delivers the greatest visual improvement for the lowest possible cost?

The engine should seek high **visual quality per unit of hardware**.

## Graphics scalability

The same project should be able to use different paths depending on hardware.

Example:

```text
LOW-END
├── Baked Lighting
├── Probes
├── PBR
├── Shadow Maps
└── lightweight effects

MID-RANGE
├── PBR
├── Forward+ / Clustered
├── SSAO
├── SSR
├── TAA
└── moderate volumetrics

HIGH-END
├── Real-time GI
├── optional Ray Tracing
├── Advanced Reflections
├── High-end Volumetrics
├── Virtualized Geometry
└── Advanced Upscaling/Reconstruction
```

The engine remains the same. The cost changes according to the features used.

# PLATFORM LAYER

The engine should have its own operating-system layer.

```text
Platform
│
├── Windows
│   └── Win32
│
├── Linux
│   ├── Wayland
│   └── X11 optional
│
└── macOS
    └── Cocoa
```

Engine API:

```text
window_create()
window_destroy()

window_poll_events()

mouse_position()
mouse_button()

keyboard_key()

filesystem_open()

thread_create()

timer_get()
```

The rest of the engine should not depend directly on the operating system.

---

# ASSET SYSTEM

Original files:

```text
PNG
JPG
TGA
GLTF
FBX
WAV
OGG
SLANG
etc.
```

They do not necessarily need to be used directly during gameplay.

Pipeline:

```text
Original file
      ↓
Asset Importer / Shader Compiler
      ↓
Asset Compiler
      ↓
Optimization / Compression / Conversion
      ↓
Optimized asset / Compiled shader
      ↓
Runtime
```

Example:

```text
dragon.glb
    ↓
dragon.mesh
```

```text
dragon.png
    ↓
dragon.texture
```

```text
lighting.slang
    ↓
SPIR-V / DXIL / Metal target
    ↓
shader cache
```

The runtime should do as little as possible.

Importing, conversion, compression, compilation and variant generation should happen offline whenever this improves the runtime.

# EDITOR

The editor must be completely separate from the runtime.

```text
Editor
   │
   ↓
Engine API
   │
   ↓
Engine Runtime
```

The exported game should not load:

* editor;
* inspector;
* asset browser;
* editor UI;
* project manager;
* importers;
* Slang compiler;
* shader compiler;
* asset compiler;
* debug tools that are not required.

The editor should remain visually modern and pleasant, but interface polish must not justify systematic waste of memory or CPU.

The editor viewport should use the same engine renderer so that the preview represents the final result correctly.

# EDITOR TARGETS

Empty project:

```text
RAM:

target:
< 150 MB
```

Startup:

```text
target:
< 1 second
```

These numbers are initial targets and may change according to real measurements.

---

# RUNTIME TARGETS

Empty project:

```text
RAM:

target:
< 20 MB
```

Startup:

```text
target:
< 100 ms
```

Base executable:

```text
target:
< 10 MB
```

These values are engineering targets, not promises. The cost of an empty project must be separated from the cost of modules and assets that the project chooses to load.

# MODULAR BUILD

The build and packaging system will allow modules to be selected.

Example:

```text
[✓] 3D Renderer
[✓] PBR
[✓] Audio
[✓] Physics
[✓] Animation
[✓] TAA

[ ] 2D Renderer
[ ] Lua
[ ] Networking
[ ] Video
[ ] Navigation
[ ] Volumetrics
[ ] Real-time GI
[ ] Ray Tracing
[ ] Virtualized Geometry
```

Final build:

```text
Game.exe

Core
3DRenderer
PBR
Audio
Physics
Animation
TAA
```

Not included:

```text
Lua
Networking
2D
Video
Navigation
Volumetrics
Real-time GI
Ray Tracing
Virtualized Geometry
Slang compiler
Editor
Asset compiler
```

The goal is not merely to disable features at runtime. Optional modules should be removed from the build, link or final package whenever technically possible. Shared components may still have some residual cost; that cost must be known and measured.

# PRINCIPLES

```text
Don't Pay For What You Don't Use
```

Or:

```text
You only pay for what you use.
```

Second principle:

```text
Maximum Visual Quality Per Unit of Hardware
```

Or:

```text
Maximum visual quality for the lowest possible cost.
```

These are central architectural rules.

The engine should not have to choose between looking good and being lightweight.

It should pursue both through architecture, scalability and modularity.

# PERFORMANCE BUDGET

Every important change should be measurable.

Automatic benchmarks:

```text
Startup Time

RAM Idle

Executable Size

RAM / 1,000 Entities

RAM / 10,000 Entities

RAM / 100,000 Entities

CPU / 1,000 Entities

CPU / 10,000 Entities

CPU / 100,000 Entities

Frame Time

GPU Time

Asset Loading Time

Scene Loading Time

Draw Calls

Build Time
```

---

# PERFORMANCE REGRESSION

The CI system should detect regressions in the future.

Example:

```text
PERFORMANCE REGRESSION

Runtime Memory

before:
31.4 MB

after:
38.7 MB

difference:
+23.2%

STATUS:
FAILED
```

Another:

```text
Startup

before:
214 ms

after:
221 ms

difference:
+3.2%

STATUS:
WARNING
```

---

# DEPENDENCIES

Rule:

> No dependency is sacred.

Every library should be evaluated by:

```text
RAM
CPU
Binary Size
Startup
Dependencies
Build Time
Maintainability
Platform Support
```

A library should not be added only because it makes development easier.

---

# VALIDATION AND EVOLUTION ROADMAP

The detailed task roadmap, dependencies and completion criteria are in [`ROADMAP.md`](ROADMAP.md). This section keeps a summary of the stages.

The versions below are technical milestones, not launch deadlines. The engine may take as long as necessary to reach quality, and each milestone may be split into several internal prototypes.

An iteration should be considered complete only when its behavior, costs and failure paths are understood through tests and measurements. Unvalidated decisions should not be treated as permanent architectural contracts.

## v0.0.1

C++ runtime only + Slang for the first shader.

```text
Core
Memory
Platform
Window
Input
Renderer
Initial RHI
Vulkan
Minimal shader pipeline
```

Objective:

```text
Open a window
Initialize the GPU
Compile/prepare a development shader
Load a compiled shader
Clear the screen
Render a triangle
Receive input
Close correctly
```

The first version should establish the separation between CPU/runtime code and GPU/shader code.

# v0.0.2

Add:

```text
Entities
Scene
Transform
Camera
Mesh
Texture
Basic Asset Loading
Basic Material
Initial PBR
```

Objective:

```text
Open the engine
Load a model
Create a camera
Apply a material
Light a basic scene
Render the model
```

The visual priority at this stage is to obtain a correct PBR foundation before complex effects.

# v0.0.3

ECS enters this phase as a decision to be validated by real use and benchmarks. The data layout, query model and storage strategy may be revised before being considered stable.

Add:

```text
ECS
Materials
Slang shader library
Shader variants/cache
Lighting
IBL
Shadows
HDR
Tone Mapping
Asset System
C ABI
```

Objective:

Create the engine's first genuinely solid visual foundation without introducing high-cost AAA features before the required infrastructure exists.

# v0.0.4

Add Rust tools:

```text
Asset Compiler
Shader Tooling
Texture Compiler
Model Importer
Initial Animation Compiler
Initial Packager
```

The pipeline should generate data ready for efficient runtime consumption.

# v0.0.5

Add:

```text
Basic Editor

Viewport
Hierarchy
Inspector
Asset Browser
Console
Profiler
Material Inspection
Development Shader Hot Reload
```

The compiler and shader tools remain development/editor components, not part of the final game.

# v0.1

This is the target for a first usable engine, not a date. Features may be released and stabilized in independent milestones.

Add:

```text
Optional Lua
Physics
Audio
Animation
Project System
Build / Export
Forward+ or Clustered Lighting
TAA
SSAO
Initial post-processing effects
```

Objective:

```text
Open the editor
Create a project
Import a model
Drag it into a scene
Create a light
Create a camera
Create a PBR material
Add a script
Press Play
Export the game
```

When this works correctly, the engine may be considered functional.

Features such as dynamic GI, ray tracing, virtualized geometry and advanced volumetrics remain for later versions, after profiling and base stability.

# OPEN DECISIONS AND EXPERIMENTS

To preserve architectural quality, some decisions must be selected through prototypes and measurements rather than preference alone:

* ECS model and query layout;
* resource handles, ownership and lifetime strategy;
* RHI limits and capability model;
* primary lighting path, such as Forward+, Clustered or Deferred;
* allocator combination for permanent, temporary and streaming data;
* asset format, cache and streaming policy;
* integration and real cost of optional Lua;
* use of compute, async compute, GPU culling and other advanced techniques;
* supported C++ standard, compilers and sanitizers.

Keeping these decisions open during research does not weaken the vision. It prevents abstractions from being frozen before their real requirements are known.

# FINAL PHILOSOPHY

The engine should follow these principles:

```text
LIGHTWEIGHT

VISUALLY ADVANCED

FAST

MODULAR

DATA ORIENTED

NO MANDATORY GC IN CORE

LOW MEMORY

LOW STARTUP TIME

LOW BINARY SIZE

COST PROPORTIONAL TO USED FEATURES

MODERN GRAPHICS

SCALABLE GRAPHICS

MULTITHREADED

GPU DRIVEN

EXPLICIT MEMORY

OFFLINE ASSET PROCESSING

OFFLINE SHADER COMPILATION

CROSS PLATFORM

SIMPLE FOR DEVELOPERS
```

Visual quality should not depend on making every project heavy.

Expensive features should have alternative paths and/or remain optional.

# OFFICIAL STACK

```text
Runtime / Core / Renderer:
C++20

Public ABI / Plugin Boundary:
C ABI

C++ SDK / Public API:
C++

Offline Tools:
Rust

Native Gameplay:
C++

Gameplay Scripting:
Optional Lua

GPU Shaders:
Slang

Build System:
CMake + toolchain presets

Primary Graphics API:
Vulkan

Future Graphics APIs:
Direct3D 12
Metal

Platforms:
Windows
Linux
macOS in the future
```

## Responsibility of each language

```text
C++
└── everything that must be lightweight, predictable and close to the runtime

C ABI
└── stable boundary for plugins and interoperability

Rust
└── offline tools, compilers, importers and heavy pipeline processing

Lua
└── optional gameplay scripting

Slang
└── code executed on the GPU and multiplatform shader generation
```

No language should be added to the project merely by preference. It must solve a concrete problem.

# MOST IMPORTANT RULE

Before adding any feature, ask:

```text
1. Does this need to exist?

2. How much RAM does it add?

3. How much CPU does it use?

4. How much GPU does it use?

5. How much VRAM does it use?

6. How much does it increase the executable?

7. How much does it increase startup time?

8. Can it be optional?

9. Can it be completely removed from the build?

10. Can it be done offline?

11. Is there a cheaper path for weak hardware?

12. How much does it improve visual quality or experience?

13. Can it be implemented more simply?
```

If the answers do not justify the cost, the feature should be reconsidered.

For graphics features, always evaluate the ratio:

```text
visual improvement
──────────────────
total cost
```

The engine should maximize this ratio.

# VISION

Create a modern game engine capable of producing visually impressive games, including quality comparable to AAA engines, without following the trend of turning every project and editor into heavy software.

The goal is to enable something like:

```text
The engine opens quickly.

The project opens quickly.

Low idle RAM.

Small runtime.

The game starts quickly.

Only necessary systems are compiled or packaged.

Shaders are prepared offline.

Weak hardware can use the editor and lighter graphics paths.

Mid-range hardware can achieve excellent visual quality with efficient techniques.

Powerful hardware can enable advanced graphics technologies.
```

Scalability vision:

```text
LOW-END
   ↓
good visual quality
low cost

MID-RANGE
   ↓
excellent visual quality
efficient modern techniques

HIGH-END
   ↓
AAA quality
optional GI / RT / volumetrics / advanced reconstruction
```

The engine should not be lightweight because it has few features.

It should be lightweight because its architecture was designed for efficiency.

The engine should not be beautiful because it wastes hardware.

It should be beautiful because its renderer uses modern techniques intelligently.

Final objective:

> The beauty of an AAA engine with an architecture built from the beginning to be lightweight, scalable and modular.
