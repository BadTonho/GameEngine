# Scene data foundation

Phase 6 adds an internal scene layer without requiring model or texture files. The Vulkan
bootstrap still renders the procedural cube and checkerboard; it now obtains its transform,
camera and directional light from a `gameengine::scene::Scene` instance.

## Runtime model

Entities are 64-bit handles composed of a 32-bit index and a 32-bit generation. Destroyed
indices may be reused, but an older generation is rejected. Components use sparse-set pools:
the sparse index provides lookup and the dense arrays provide iteration-friendly storage.

The initial component set is deliberately small:

- `TransformComponent`: local position, quaternion, scale, parent and cached world matrix;
- `MeshRendererComponent`: internal mesh and material IDs;
- `CameraComponent`: perspective parameters and active-camera flag;
- `DirectionalLightComponent`: direction, color and intensity.

Hierarchy uses local TRS plus a parent. Self-parenting, missing parents and cycles are rejected.
Destroying a parent detaches its children as roots. The renderer currently creates one procedural
cube entity, one camera and one directional light during initialization.

## `.gescene` graph chunks

The existing version-1 flat `SCHD` + `INST` representation remains readable. New scenes use
the following deterministic chunks, ordered by entity index:

| Chunk | Record | Purpose |
| --- | ---: | --- |
| `SCHD` | 16 bytes | entity count, flags, active camera handle |
| `ENTS` | 16 bytes | entity handle and parent handle |
| `TRNS` | 48 bytes | entity handle, position, quaternion and scale |
| `MESH` | 24 bytes | entity handle, mesh ID and material ID |
| `CAMR` | 24 bytes | entity handle, FOV, near/far planes and active flag |
| `LITE` | 40 bytes | entity handle, direction, color, intensity and reserved data |

The C++ serializer and the Rust offline packer operate in memory and retain the existing
little-endian, 16-byte-aligned, FNV-1a-hashed container rules. The C++ reader exposes spans over
the original bytes; it does not allocate an asset-owned copy.

## Measurements and limits

The optional `gameengine_scene_benchmark` target measures synthetic scenes with 1,000, 10,000
and 100,000 entities. It reports creation, hierarchy update, query, destruction and estimated
pool memory. The numbers are machine-specific and are recorded for comparison, not as a CI
threshold.

Build it with:

```text
cmake --preset windows-msvc-debug -DGAMEENGINE_BUILD_BENCHMARKS=ON
cmake --build build/windows-msvc-debug --config Debug --target gameengine_scene_benchmark
build/windows-msvc-debug/Debug/gameengine_scene_benchmark.exe
```

The separate `gameengine_editor` target can create and open the procedural project, load this
scene into the same internal `Scene` representation and attach it to the Vulkan renderer through
an editor-only bridge. The renderer reads the selected scene's cube transform, active camera and
directional light without changing the public RHI or C ABI. Project manifests validate the scene
path before resolving it, and a failed load does not replace the previous valid scene.

Prepared meshes, real textures, runtime asset streaming, animation, camera navigation, asset
browser and a final ECS storage choice remain future work.
