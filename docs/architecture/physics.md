# Physics architecture

Phase 9C adds `gameengine_physics` as an optional C++ runtime module backed by Jolt Physics (v5.6.0).
It does not change the RHI, the C ABI, the asset formats or the Rust tooling workspace. A build with
`GAMEENGINE_BUILD_PHYSICS=OFF` does not link the module and excludes all Jolt code from the executable.

## Runtime model

The physics system is owned by `gameengine::physics::PhysicsWorld`. It provides a clean, engine-facing
API using engine math types (`Vec3`, `Quaternion`) and handles (`scene::Entity`).

- **Shapes**: Box (half-extents) and Sphere (radius). Primitive shapes are created using embedded shape
  settings in Jolt to minimize allocation overhead. Shapes smaller than 0.05 units are rejected.
- **Motion types**: Static bodies (infinite mass, stationary colliders) and Dynamic bodies (simulated
  under gravity with mass, friction, restitution and inertia calculation).
- **Entities**: Bodies map 1:1 to root entities in `scene::Scene`. Hierarchical entities with parents
  are rejected. Entities must be alive and have valid initial `TransformComponent` data.
- **Fixed-step simulation**: The simulation steps at a deterministic fixed time delta (default 1/60 s).
  A frame-time accumulator handles variable frame rates. If accumulated delta exceeds
  `max_steps_per_update`, excess time is dropped and recorded in `PhysicsMetrics::dropped_steps` to
  prevent death-spiral degradation on frame spikes.
- **Transform synchronization**: `write_transforms(scene)` copies simulated positions and rotations
  into `scene::TransformComponent::local_position` and `local_rotation`, followed by updating world
  matrices. If an entity was destroyed or invalidated before syncing, the write fails without partial
  state application.

## Library isolation and third-party rules

- **Private dependency**: Jolt Physics headers and symbols are encapsulated entirely within `physics.cpp`.
  The public header `src/engine/physics/physics.hpp` contains zero Jolt headers or types (PIMPL pattern).
- **Build and compilation**: Jolt is integrated via CMake's `FetchContent` pinned to release tag `v5.6.0`
  with SHA-256 verification. Offline building is supported by configuring `FETCHCONTENT_SOURCE_DIR_JOLTPHYSICS`.
- **Options**: Exceptions and RTTI are disabled (`CPP_EXCEPTIONS_ENABLED=OFF`, `CPP_RTTI_ENABLED=OFF`).
  DirectX, Vulkan, and Metal compute backends in Jolt are disabled to prevent graphics API leakage across
  the RHI boundary. Fast math and non-standard SIMD are disabled for consistent baseline behavior.
- **Threading**: Single-threaded job system (`JobSystemSingleThreaded`) is used in this initial phase.
  No background threads or global worker pools are spawned casually, strictly adhering to AGENTS.md rule 19.
- **Memory**: Body counts and temporary allocator buffers are sized during `initialize()`. `update()`
  and `write_transforms()` perform zero dynamic heap allocations.

## Runtime integration and validation

- `--physics-demo`: Runtime argument that pauses procedural transform animation, sets up a ground
  plate and drops the bootstrap cube from height under gravity.
- `--smoke-test --physics-demo`: Runs one frame of the physics demo to validate clean initialization,
  simulation step and teardown without leaks or errors.
- **Determinism**: Identical configurations and step inputs produce identical simulation transforms
  on the same platform.

## Benchmarks and cost

The optional benchmark executable `gameengine_physics_benchmark` (available when `GAMEENGINE_BUILD_BENCHMARKS=ON`)
measures update time and step metrics across 1,000 and 10,000 dynamic falling box bodies.

## Future work

Constraints and joints, raycasting and sweeps, character controllers, convex hulls, mesh colliders,
continuous collision detection (CCD), and editor authoring/gizmos remain future milestones.
