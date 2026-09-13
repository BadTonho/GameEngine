# AGENTS.md

## Project Mission

This project is a modern, extremely lightweight, high-performance and modular game engine.

The primary goal is not to maximize the number of features.

The primary goal is to provide modern game-engine capabilities while minimizing:

* RAM usage
* CPU overhead
* startup time
* executable size
* unnecessary allocations
* dependencies
* runtime complexity
* build overhead

The engine must remain usable on low-end hardware while still supporting modern rendering and engine technologies.

The main architectural principle is:

> You do not pay for what you do not use.

Optional systems must be removable from the final build whenever technically possible.

---

# Official Technology Stack

## Runtime

Primary language:

```text
Zig
```

Zig should be used by default for runtime systems.

Examples:

* Core
* Memory
* Platform
* Window
* Input
* Filesystem
* Jobs
* Threading
* Renderer
* RHI
* Scene
* ECS
* Assets runtime
* Animation runtime
* Audio integration
* Physics integration
* Networking runtime
* Game runtime

Do not introduce another runtime language without a strong technical reason.

---

## Public ABI

Use:

```text
C ABI
```

The public binary interface should remain simple and language-independent whenever possible.

Do not expose internal Zig implementation details through the public ABI.

Prefer:

```c
typedef uint64_t EngineEntity;

EngineEntity engine_entity_create(void);
void engine_entity_destroy(EngineEntity entity);
```

Avoid exposing:

* language-specific containers
* internal pointers
* compiler-specific structures
* implementation-specific ownership
* unstable layouts

The C ABI exists to allow future interoperability with:

* C
* C++
* Zig
* Rust
* C#
* Lua
* other languages

---

# Rust Usage

Rust is primarily intended for offline tools and development infrastructure.

Preferred Rust use cases:

* Asset compiler
* Shader compiler
* Texture compiler
* Model importer
* Animation compiler
* Asset database
* Packaging tools
* Build tools
* Project tools
* Development utilities

Do not add Rust to the game runtime unless there is a clear measurable advantage.

Runtime simplicity has priority.

---

# Lua Usage

Lua is optional.

Lua may be used for gameplay scripting.

Lua must never become a mandatory dependency of the engine runtime.

Projects that do not use Lua should not include the Lua runtime in the final executable.

---

# Core Philosophy

Every implementation should optimize for:

```text
low memory usage
low CPU overhead
low startup time
small binaries
predictable performance
modularity
explicit resource ownership
simple architecture
minimal dependencies
fast iteration
```

Do not optimize only for developer convenience if it causes permanent runtime overhead.

---

# Zero-Cost Optional Features

Features must be modular whenever reasonably possible.

Examples:

```text
Physics
Audio
Lua
Networking
Navigation
Video
Ray Tracing
2D Renderer
3D Renderer
Editor-only systems
Debug systems
```

If a project does not use a module, prefer removing it entirely from the build.

Do not merely disable unused systems at runtime when they can be excluded at compile time.

Preferred:

```text
Feature not used
↓
Feature not compiled
↓
Feature contributes approximately zero runtime cost
```

---

# Runtime vs Editor

The editor and runtime must remain separated.

The exported game must not contain editor systems unless explicitly required.

Editor-only functionality includes:

* Inspector
* Hierarchy UI
* Asset Browser
* Editor Viewport UI
* Project Manager
* Import interfaces
* Debug editor panels
* Development consoles
* Editor-specific metadata
* Asset conversion tools

Do not introduce editor dependencies into the runtime.

---

# Runtime vs Offline Tools

Expensive work should preferably happen before runtime.

Prefer:

```text
Source Asset
↓
Offline Compiler
↓
Optimized Engine Asset
↓
Runtime
```

instead of:

```text
Source Asset
↓
Heavy processing during game startup
```

The runtime should consume data that is already prepared for efficient loading.

---

# Memory Rules

Memory management must be explicit.

Avoid unnecessary heap allocation.

Do not allocate every frame unless necessary.

Target:

```text
0 dynamic heap allocations during normal frame execution
```

This is a target, not an absolute rule.

If an allocation is necessary, understand and document why.

Prefer:

* arenas
* pools
* fixed-capacity structures where appropriate
* frame allocators
* scratch allocators
* contiguous storage
* reuse of existing allocations

Avoid repeated:

```text
allocate
free
allocate
free
```

inside hot paths.

---

# Allocation Ownership

Every significant allocation must have clear ownership.

It should be obvious:

* who allocated memory
* who owns it
* when it becomes invalid
* who releases it
* whether it can move
* whether references remain stable

Avoid ambiguous ownership.

---

# Frame Allocators

Temporary per-frame data should prefer frame/scratch allocators.

Typical structure:

```text
Permanent Arena
Engine Arena
Scene Arena
Asset Arena
Frame Arena A
Frame Arena B
```

Frame memory should be reusable rather than repeatedly allocated and freed.

---

# Data-Oriented Design

Prefer data-oriented structures for performance-critical systems.

Prefer contiguous data:

```text
Transforms:
[T][T][T][T][T][T]

Velocities:
[V][V][V][V]

Renderables:
[R][R][R][R]

Lights:
[L][L][L]
```

over deeply nested object graphs where the latter provides no meaningful benefit.

Consider:

* CPU cache locality
* SIMD
* batching
* predictable iteration
* multithreading
* GPU uploads
* memory footprint

Do not use ECS or DOD blindly.

Use them where they provide measurable architectural or performance benefits.

---

# Entities

Entities should preferably be lightweight IDs or handles.

Do not make entities large owning objects.

A preferred design is a generational handle.

Example:

```text
Entity ID

Generation | Index
```

Destroyed entities should not leave silently valid stale handles.

---

# Hot Paths

Hot paths require special care.

Examples:

* frame update
* rendering
* entity iteration
* animation
* physics synchronization
* visibility
* asset streaming
* job scheduling

Inside hot paths:

* avoid unnecessary allocation
* avoid unnecessary indirection
* avoid unnecessary locks
* avoid unnecessary virtual dispatch
* avoid repeated string processing
* avoid filesystem access
* avoid logging spam
* avoid hidden copies

Performance-critical decisions should be benchmarked.

---

# Renderer Architecture

Do not spread Vulkan, Direct3D or Metal calls throughout the engine.

Use a rendering abstraction.

Preferred architecture:

```text
Scene
↓
Renderer
↓
Render Graph
↓
RHI
↓
Graphics Backend
```

Graphics backends may include:

```text
Vulkan
Direct3D 12
Metal
```

The rest of the engine should not depend directly on a specific graphics API.

---

# RHI

The Render Hardware Interface should expose engine concepts such as:

```text
GPUDevice
GPUBuffer
GPUTexture
GPUSampler
GPUShader
GPUPipeline
GPUCommandList
GPUFence
GPUSemaphore
GPUSwapchain
```

Keep the RHI:

* small
* explicit
* predictable
* low overhead

Do not attempt to hide every difference between graphics APIs.

Abstract what is useful, not everything.

---

# Graphics Priorities

Initial graphics backend:

```text
Vulkan
```

Possible future backends:

```text
Direct3D 12
Metal
```

Modern rendering features may include:

* GPU-driven rendering
* indirect drawing
* bindless resources
* compute shaders
* async compute
* render graphs
* GPU culling
* PBR
* Forward+
* clustered lighting
* HDR
* temporal anti-aliasing
* upscaling
* GPU particles
* texture streaming
* mesh streaming

Do not implement a feature merely because it is considered modern.

Every feature must justify:

* complexity
* memory cost
* CPU cost
* GPU cost
* maintenance cost
* usefulness

---

# Platform Layer

Operating-system-specific code must be isolated.

Preferred structure:

```text
platform/
├── windows/
├── linux/
└── macos/
```

Possible implementations:

```text
Windows → Win32
Linux → Wayland
Linux compatibility → X11 when required
macOS → Cocoa
```

The rest of the engine should use a platform-independent internal API.

---

# Dependencies

Dependencies must be treated as costs.

Before adding a dependency, evaluate:

* binary size
* runtime memory
* startup cost
* CPU overhead
* transitive dependencies
* compilation time
* platform support
* maintenance status
* API stability
* licensing
* whether only a small portion of it is required

Do not add a large dependency to solve a small problem without considering alternatives.

No dependency is sacred.

---

# Dependency Removal

Whenever reasonable, systems should be designed so dependencies can later be replaced.

Avoid spreading third-party APIs throughout the entire codebase.

Wrap external libraries behind internal interfaces when doing so provides meaningful isolation.

---

# Simplicity

Prefer simple solutions over clever solutions.

Avoid unnecessary abstraction layers.

Avoid architecture designed for hypothetical future requirements.

Do not create a generic framework when the engine currently needs one concrete implementation.

Build the simplest architecture that preserves the important long-term boundaries.

---

# Premature Optimization

The project is performance-oriented, but performance decisions should still be based on evidence.

Do not destroy code clarity for insignificant theoretical optimizations.

Measure when possible.

Optimize:

```text
measured bottlenecks
architectural overhead
memory usage
critical loops
startup
loading
binary size
```

not imaginary problems.

---

# Benchmarking

Performance is a feature.

Important systems should have benchmarks whenever practical.

Track:

* runtime RAM
* editor RAM
* startup time
* executable size
* scene loading time
* asset loading time
* frame time
* CPU time
* GPU time
* entity update performance
* allocations per frame
* build time

Suggested entity benchmarks:

```text
1,000 entities
10,000 entities
100,000 entities
1,000,000 entities when relevant
```

---

# Performance Regressions

Do not accept significant performance regressions without understanding them.

When changing critical systems, compare before and after when possible.

Example:

```text
Before:
31.4 MB

After:
38.7 MB

Regression:
+23.2%
```

A regression like this requires investigation or justification.

---

# Performance Budgets

Initial targets may include:

```text
Empty Runtime RAM:
< 20 MB

Empty Runtime Startup:
< 100 ms

Base Runtime Executable:
< 10 MB

Empty Editor RAM:
< 150 MB

Editor Startup:
< 1 second
```

These are engineering targets, not guaranteed specifications.

Do not fake optimizations merely to meet a target.

Real-world usefulness comes first.

---

# Logging

Logging must not introduce significant hot-path overhead.

Avoid formatting expensive log messages that will not be emitted.

Logging levels should include concepts similar to:

```text
trace
debug
info
warning
error
fatal
```

Release builds should be able to remove unnecessary logging.

---

# Error Handling

Errors must be handled explicitly.

Do not silently ignore failures.

Avoid crashing for recoverable runtime situations.

For programmer errors and broken invariants, assertions are acceptable.

Separate:

```text
user/data errors
recoverable runtime errors
programmer errors
fatal engine errors
```

---

# Assertions

Assertions are encouraged for internal invariants.

Examples:

```text
invalid handle
out-of-range internal index
broken ownership
impossible renderer state
corrupted resource state
```

Assertions must not replace proper handling of normal user-generated errors.

---

# Strings

Avoid unnecessary string manipulation in performance-sensitive runtime code.

Prefer:

* IDs
* handles
* hashes when appropriate
* interned strings when appropriate

Do not prematurely hash everything.

Collisions must be handled safely if hashes are used as identifiers.

---

# File Formats

Runtime formats should favor:

* fast loading
* minimal parsing
* alignment suitable for runtime structures
* versioning
* validation
* forward evolution

Do not depend on raw development formats forever.

---

# Asset IDs

Runtime assets should use stable handles or IDs rather than relying entirely on filesystem paths.

Paths are useful during development, but runtime systems should be designed for efficient asset lookup.

---

# Asset Loading

Asset loading should support asynchronous operation where beneficial.

Do not block the entire main thread for operations that can safely happen elsewhere.

However, do not create asynchronous complexity when the operation is trivial.

---

# Multithreading

The engine should be designed with multithreading in mind.

Potential parallel systems include:

* asset loading
* asset processing
* animation
* visibility
* physics
* rendering preparation
* background streaming
* scene processing

Avoid excessive fine-grained synchronization.

Prefer jobs with clear ownership and dependencies.

---

# Job System

The job system should be lightweight.

Avoid creating operating-system threads per task.

Prefer a worker pool.

Important properties:

* low scheduling overhead
* predictable synchronization
* ability to express dependencies
* minimal allocation
* good debugging support

---

# Locking

Avoid global locks.

Avoid holding locks across expensive operations.

Prefer:

* immutable data
* ownership transfer
* job dependencies
* double buffering
* per-thread storage
* lock-free techniques only when justified

Do not use lock-free programming merely because it sounds faster.

---

# Cache Locality

Consider cache locality when designing performance-critical structures.

Avoid pointer chasing when contiguous structures can solve the problem.

Measure when architectural tradeoffs are uncertain.

---

# SIMD

SIMD may be used where it provides measurable benefits.

Do not manually vectorize everything.

Prefer clean data layouts that naturally allow the compiler or specialized routines to vectorize important loops.

---

# Editor Philosophy

The editor itself must also remain lightweight.

Do not assume editor performance is irrelevant because it is a development tool.

Optimize:

* idle RAM
* startup
* viewport responsiveness
* asset browser scalability
* project loading
* scene editing
* UI redraw
* background tasks

The editor should remain usable on modest computers.

---

# User Experience

Lightweight does not mean difficult to use.

The engine should be simple for game developers.

Do not expose internal low-level complexity when a clean high-level API can provide the same performance.

The user-facing API should prioritize:

* clarity
* consistency
* discoverability
* predictable behavior

---

# Internal vs Public API

Internal APIs may change aggressively during early development.

Public APIs should be introduced carefully.

Do not promise API stability too early.

Once something becomes officially public, breaking changes should require deliberate consideration.

---

# API Design

Prefer APIs that make ownership and cost visible.

Avoid APIs that hide expensive work.

If an operation can:

* allocate
* block
* access disk
* synchronize threads
* upload GPU data

that behavior should be reasonably clear from the API or documentation.

---

# Naming

Use clear and descriptive names.

Do not use unnecessarily abbreviated names.

Acceptable common abbreviations include:

```text
GPU
CPU
RHI
ECS
API
ABI
ID
IO
UI
```

Prefer consistency over personal naming style.

---

# Comments

Comments should explain:

* why something exists
* important invariants
* non-obvious performance decisions
* platform limitations
* unusual algorithms
* ownership assumptions

Do not write comments that merely repeat the code.

Bad:

```text
increment index
```

Good:

```text
Generation increments when a slot is reused so stale entity handles
cannot reference the new entity occupying the same index.
```

---

# Documentation

Architecturally important systems must have documentation.

Document at minimum:

* purpose
* ownership
* lifecycle
* threading assumptions
* memory behavior
* public interfaces
* major constraints

---

# Source Layout

Keep subsystem boundaries clear.

Preferred structure:

```text
engine/
├── core/
├── platform/
├── window/
├── input/
├── filesystem/
├── renderer/
├── scene/
├── ecs/
├── assets/
├── animation/
├── audio/
├── physics/
├── scripting/
└── runtime/
```

Do not create arbitrary cross-dependencies between subsystems.

---

# Module Dependencies

Prefer one-directional dependencies.

Avoid circular dependencies.

Example:

```text
Core
↑
Platform
↑
Renderer
↑
Scene
```

is easier to reason about than every subsystem directly importing every other subsystem.

When circular architecture appears necessary, reconsider the ownership boundary.

---

# Build Configuration

Support at minimum concepts equivalent to:

```text
Debug
Development
Release
```

Debug:

* assertions
* validation
* extensive diagnostics

Development:

* useful diagnostics
* profiling
* reasonable optimization

Release:

* optimization
* minimal diagnostics
* unnecessary systems stripped

---

# Debug Code

Debug tools must be removable from release builds.

Examples:

* debug overlays
* validation layers
* profiling labels
* verbose logs
* debug visualizers

Do not make release games permanently pay for development-only systems.

---

# Profiling

Performance-critical systems should be designed to support profiling.

CPU and GPU markers should be available without permanently adding significant overhead to production builds.

---

# Tests

Core systems should have automated tests where practical.

Important test targets include:

* allocators
* containers
* entity handles
* serialization
* asset formats
* math
* resource lifecycle
* job synchronization
* filesystem utilities

---

# Correctness Before Optimization

Never knowingly introduce memory corruption, undefined behavior or race conditions merely to improve benchmark numbers.

Priority:

```text
Correctness
↓
Architecture
↓
Measurement
↓
Optimization
```

---

# Cross-Platform Rules

Do not introduce unnecessary platform assumptions into shared code.

Platform-specific behavior belongs inside platform-specific modules.

When behavior differs between operating systems, expose a clean internal abstraction rather than spreading conditional compilation everywhere.

---

# Security and Input Validation

External files must be treated as untrusted input.

Asset importers and runtime loaders must validate:

* sizes
* offsets
* versions
* counts
* boundaries
* integer overflow
* malformed data

Do not assume asset files are valid.

---

# No Hidden Work

Avoid APIs that unexpectedly perform large amounts of work.

Examples of hidden work to avoid:

```text
implicit disk access
implicit GPU synchronization
implicit heap allocation
implicit copies of large resources
implicit resource compilation
```

Expensive operations should be explicit when practical.

---

# No Feature Creep

Before implementing a new major system, ask:

1. Is this necessary for the current engine goals?
2. Does it belong in the engine core?
3. Can it be a module?
4. Can it be a plugin?
5. Can it be an offline tool?
6. Can it be implemented later?
7. What is its runtime cost?
8. What is its memory cost?
9. What dependency does it introduce?
10. Does a simpler implementation solve the actual problem?

---

# Before Adding a Dependency

Ask:

1. What exact problem does it solve?
2. How much code from the dependency is actually needed?
3. What does it add to binary size?
4. What does it add to memory?
5. Does it allocate internally?
6. Does it create threads?
7. Does it have a runtime?
8. What dependencies does it bring?
9. Is it actively maintained?
10. Can it be replaced later?

---

# Before Adding an Abstraction

Ask:

1. Is there currently more than one implementation?
2. Is another implementation actually planned?
3. Does the abstraction hide important performance characteristics?
4. Does it add indirect calls?
5. Does it make ownership harder to understand?
6. Is the abstraction simpler than the code it replaces?

---

# When Modifying Performance-Critical Code

Before finalizing:

1. Confirm correctness.
2. Compile the relevant target.
3. Run available tests.
4. Run relevant benchmarks when practical.
5. Check allocations.
6. Check memory impact.
7. Check binary impact when significant.
8. Compare with the previous implementation when possible.

---

# Agent Rules

When working on this repository, agents must:

* inspect existing architecture before creating new systems;
* reuse existing conventions;
* avoid unnecessary dependencies;
* avoid unnecessary new abstraction layers;
* keep runtime code lightweight;
* keep editor code out of runtime modules;
* keep offline processing out of runtime whenever practical;
* preserve module boundaries;
* avoid circular dependencies;
* consider memory ownership explicitly;
* consider threading implications;
* consider binary-size implications;
* consider startup implications;
* benchmark performance-sensitive changes when possible;
* add tests for important logic;
* document non-obvious architectural decisions.

---

# Agent Must Not

Agents must not:

* replace working systems purely based on personal preference;
* add C++, C#, Java, Python or another runtime language without a concrete architectural reason;
* add large frameworks for convenience;
* make Lua mandatory;
* mix editor and runtime code;
* introduce garbage collection into the engine core;
* add background threads without documenting ownership and shutdown;
* allocate memory every frame unnecessarily;
* use global mutable state without strong justification;
* hide major runtime costs behind innocent-looking APIs;
* introduce platform-specific code into common modules unnecessarily;
* optimize by guessing when measurement is available;
* sacrifice correctness for benchmark results;
* implement speculative features unrelated to current milestones.

---

# Agent Decision Priority

When multiple solutions are valid, prefer in this order:

```text
1. Correctness
2. Simplicity
3. Low runtime overhead
4. Low memory usage
5. Clear ownership
6. Modularity
7. Maintainability
8. Small binary size
9. Fast startup
10. Developer convenience
```

Developer convenience is important, but it must not silently compromise the central goals of the engine.

---

# Current Development Strategy

Do not attempt to build the entire engine at once.

Initial development order:

```text
v0.0.1

Core
Memory
Platform
Window
Input
Vulkan Renderer
```

Target:

```text
create window
initialize GPU
clear screen
render triangle
receive input
shutdown cleanly
```

Then:

```text
v0.0.2

Entities
Scene
Transform
Camera
Mesh
Texture
Basic Assets
```

Then:

```text
v0.0.3

ECS
Materials
Shaders
Lighting
Asset System
C ABI
```

Then:

```text
v0.0.4

Rust offline tools
```

Then:

```text
v0.0.5

Basic Editor
```

Then:

```text
v0.1

Lua
Physics
Audio
Animation
Project System
Build / Export
```

Do not skip foundational architecture to prematurely build editor features.

---

# Definition of Done

A change is not considered complete merely because it compiles.

For important changes, verify as applicable:

```text
compiles
runs
tests pass
no obvious memory leaks
no invalid resource lifetime
shutdown works correctly
error paths work
performance remains acceptable
module boundaries remain clean
documentation is updated
```

---

# Final Principle

The engine must not be lightweight because it lacks functionality.

The engine must be lightweight because functionality is designed efficiently.

Every subsystem should respect:

> Modern technology without unnecessary weight.

And:

> You only pay for what you use.
