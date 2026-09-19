# Optional transform animation

Phase 9A adds a small C++ animation module without making animation part of the RHI or
the C ABI. The module is built as `gameengine_animation` when
`GAMEENGINE_BUILD_ANIMATION=ON` and can be removed from a runtime build with that option
disabled.

## Runtime model

`AnimationClip` contains non-owning spans of `TransformTrack` and keyframes. A track targets a
generation-checked `scene::Entity` and stores position, quaternion rotation and scale values.
Clips require finite values, a duration greater than zero, keyframes sorted strictly by time, and
keyframes at both time zero and the clip duration.

`AnimationSystem` owns player state and keeps players ordered by entity index. Clips and keyframe
storage are prepared before the frame loop; `update()` does not allocate or access the filesystem.
Position and scale use linear interpolation. Rotations use normalized quaternion interpolation with
the shortest path.

The default procedural clip targets the bootstrap cube, lasts two seconds and loops. It moves the
cube slightly on Y, changes scale at the midpoint and performs a deterministic rotation. No model,
texture, animation file or external asset is required.

## Time and editor behavior

The runtime and editor pass the measured `Clock::tick()` delta. Tests can select fixed-step mode
with a 1/60 second step and a deterministic maximum step count. A stale entity handle or invalid
scene causes the update to fail before the player loop proceeds.

The editor starts the procedural clip automatically. Keyboard transform editing pauses the player
before applying the edit, and `Ctrl+S` saves the current pose. The clip name, playback state and
time are shown in the inspector, but animation state and keyframes are not serialized into
`.gescene` yet.

## Cost and removal path

Player state is preallocated through `reserve_players()`. The optional benchmark measures update
time and approximate scene/system memory for 1,000, 10,000 and 100,000 transform tracks:

```text
cmake --preset windows-msvc-debug -DGAMEENGINE_BUILD_BENCHMARKS=ON
cmake --build build/windows-msvc-debug --config Debug --target gameengine_animation_benchmark
build/windows-msvc-debug/Debug/gameengine_animation_benchmark.exe
```

When `GAMEENGINE_BUILD_ANIMATION=OFF`, the runtime and editor keep the procedural scene static and
do not link `gameengine_animation`. Skeletons, skinning, animation assets, timelines and keyframe
authoring remain future work.
