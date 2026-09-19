# Vulkan renderer foundation

Phase 2 added the first renderer path without exposing Vulkan types to shared engine headers. The Phase 4 vertical slice extends it with a procedural indexed cube, camera transforms and depth testing while keeping those scene details internal.

## Ownership

`gameengine_renderer` owns the Vulkan instance, debug messenger, surface, physical-device selection, logical device, queues, command pool, swapchain, image views, render pass, pipeline, framebuffers, command buffers and synchronization objects.

The renderer is non-copyable. Its `shutdown()` waits for the device only during explicit destruction, releases swapchain resources before the device, destroys the surface before the instance and destroys the validation messenger before the instance. No renderer resource is destroyed from the normal per-frame path.

Buffers, images, samplers and pipelines are addressed by typed index-plus-generation handles. A destroyed handle becomes invalid immediately; its Vulkan object remains pending until the frame fence associated with the destruction has signaled. Resource memory uses direct `VkDeviceMemory`; each resource owns its allocation in this phase and no custom allocator or VMA dependency is used.

The platform owns the X11 display and window. It exposes only opaque native handles through `NativeWindowHandles`; the Vulkan backend translates those handles into `VkXlibSurfaceCreateInfoKHR`.

## Frame lifecycle

Two frames in flight are used. A frame waits for its fence, acquires a swapchain image, records a command buffer, submits it to the graphics queue and presents it on the presentation queue.

`VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` trigger explicit swapchain recreation. A zero-sized window skips rendering until it has a valid extent. `VK_ERROR_SURFACE_LOST_KHR` returns a status to the runtime, which performs an orderly renderer shutdown.

The integration test resizes the X11 window, pumps the resulting configure event and renders again through the recreated swapchain. The bootstrap scene now renders a procedural indexed cube with a static camera, procedural material and a recreated depth attachment.

The renderer builds a small internal render graph for each renderer instance. Low contains the
imported swapchain color/depth resources and one deterministic `forward_opaque` pass. Medium adds
persistent procedural point-light data and a `light_list_build` compute pass over 16x16 tiles.
High adds a persistent 1024² directional shadow pass and a generated 64² mipmapped cubemap, and
degrades to Medium when those resources are unavailable. In
opt-in GPU mode, it adds storage, vertex and indirect resources and the
dependency `gpu_cull → forward_opaque`. The frame flow is:

```text
scene → visibility mode → CPU culling or gpu_cull → indirect/forward_opaque → Vulkan command buffer
```

The graph validates resource ownership, explicit and resource-derived dependencies, duplicate
writes and cycles before command recording. It remains an internal scheduler and does not allocate
transient Vulkan resources. GPU culling reads a persistent 80-byte source record per procedural
instance, atomically compacts visible model matrices into a device-local per-frame vertex slice,
and writes one `VkDrawIndexedIndirectCommand`. The GPU output order is intentionally not a
contract; the opaque depth-tested bootstrap material is order-independent. CPU culling remains the
default and the fallback when compute support or GPU resources are unavailable.

Vertex and image uploads use temporary host-visible, coherent staging buffers and one-time command buffers. Completion waits on a dedicated fence, never on `vkDeviceWaitIdle` during normal frame submission. The bootstrap mesh uses a device-local vertex buffer with position, normal and UV attributes plus a device-local index buffer. A fixed 64x64 checkerboard image and linear sampler are generated in memory and bound to the material; no mesh or texture file is loaded.

## Validation and shaders

Debug builds require `VK_LAYER_KHRONOS_validation` and enable `VK_EXT_debug_utils`. Validation errors are logged and cause the smoke test to fail. Release builds do not require validation layers. Debug object names and command labels are provided through the same extension without a RenderDoc dependency.

The bootstrap cube uses precompiled SPIR-V generated from `assets/shaders/bootstrap/triangle.slang` by `scripts/compile_bootstrap_shaders.cmake`. The offline target requires the pinned `slangc` version `2026.13.1-1-g84792eb15`, emits reflection JSON and generates the checked-in `src/engine/renderer/vulkan/triangle_shaders.hpp` header. The compiler is never a runtime dependency.

The generator records the source SHA-256, Slang version, target/profile, stage, entry point, build configuration and required capabilities in a canonical manifest. The SHA-256 of that manifest is the shader ID. Offline artifacts are cached in `build/shader-cache/Debug/<shader-id>/` or `build/shader-cache/Release/<shader-id>/`; cache hits validate the existing SPIR-V and reflection before reusing them. The generated header remains the runtime fallback for clean clones and contains the artifact table consumed by Vulkan.

The bootstrap shaders have explicit `vertex_main`, `fragment_main`, `compute_main`,
`forward_plus_vertex_main`, `forward_plus_fragment_main`,
`forward_plus_high_vertex_main`, `forward_plus_high_fragment_main`,
`forward_plus_light_list_main`, `shadow_vertex_main` and `environment_compute_main` entry points.
The compute shader uses a fixed `[numthreads(64, 1, 1)]` group, storage bindings for source,
visible and indirect records, and 112 bytes of frustum/count push constants. Vertex position,
normal and UV use Vulkan locations 0, 1 and 2; instance model columns use locations 3, 4, 5 and 6
with instance rate. The vertex shader receives only the 64-byte view-projection push constant.
Descriptor set 0 contains a material uniform buffer at binding 0, sampled image at binding 1 and
sampler at binding 2. The compiler uses the Vulkan 1.0-compatible SPIR-V 1.0 profile with the
`GLSL_450` capability and the `-emit-spirv-via-glsl` path because the pinned Slang build requires
that GLSL capability for push constants, while direct emission can emit `SPV_GOOGLE_hlsl_functionality1`,
which is not enabled by the initial Vulkan device configuration.

To regenerate the artifacts:

```text
cmake --build build/<preset> --target gameengine_compile_bootstrap_shaders --config Debug
```

The normal runtime build consumes only the generated header. The vertex input is
`position3_normal3_uv2+instance_model4`; the runtime never loads shader source or reflection JSON.
Vulkan exposes an internal capability mask and selects the highest-quality compatible variant
before creating a pipeline; the Vulkan 1.0 variant is mandatory.

The first 3D scene uses internal `Vec2`/`Vec3`/`Mat4` helpers with a right-handed camera looking toward `-Z`, Vulkan depth range `0..1` and a deterministic procedural cube. Its fragment shader applies a small metallic-roughness PBR path with directional lighting, an ambient term and deterministic tone mapping. It intentionally does not introduce a public scene, material or asset API yet. The cube's vertex and index buffers remain device-local and are uploaded through the existing staging path.

The swapchain render pass has a color attachment and a device-selected depth attachment. The renderer prefers `D32_SFLOAT`, then falls back to `D24_UNORM_S8_UINT` or `D16_UNORM`; depth is cleared to `1.0` and tested/written with `VK_COMPARE_OP_LESS`. Depth resources, framebuffers and indexed command buffers are recreated with the swapchain.

`VkPipelineCache` is loaded after device creation and before graphics pipelines. Its payload is wrapped in a device-specific GameEngine header containing the vendor/device IDs, driver version, Vulkan API version and pipeline-cache UUID. Missing, truncated, incompatible or corrupt files are ignored. On shutdown, the cache is written through a temporary file and final replacement after the device is idle. The file is never accessed during frame submission.

The C++ `RendererConfiguration` selects the pipeline-cache path and can enable explicit shader reload. Hot reload is rejected in Release builds, has no per-frame polling, waits for the device, builds replacement pipelines and swaps them only after every replacement succeeds. A failed reload leaves the active pipelines untouched. The C ABI is unchanged.

The renderer records a fixed-size timing report for each completed frame. CPU pass recording and
CPU frustum culling use
`steady_clock`. GPU timing uses two timestamp queries per timed pass and frame-in-flight only when the graphics
queue exposes timestamp bits, a valid timestamp period and a usable query reset function. Devices
without that combination continue normally and report CPU timings with GPU timing unavailable.
The development executable runs a deterministic warmup and measurement sequence with:

```text
gameengine_runtime --metrics
```

The command waits for the device, resolves pending queries and prints one independent report for
1k, 10k and 100k procedural instances, including visible/culled counts, draw calls and
average/minimum/maximum CPU and GPU time for `forward_opaque`. With `--gpu-culling`, the report
also includes `gpu_cull`, visible counts read after the frame fence, reserved source/visible/indirect
buffer sizes and the CPU fallback state. The diagnostic bridge is internal; the public RHI and C
ABI do not expose the report. Production quality is selected internally with
`--renderer-quality low|medium|high` and defaults to Medium. Low is the mandatory directional
light fallback; Medium is the Forward+ profile with deterministic 16x16 tile preparation.
Clustered and Deferred remain isolated to the benchmark. High executes `shadow_depth`, samples the
shadow map with a deterministic depth test and samples the procedural cubemap; unavailable
resources degrade to Medium or the analytic environment fallback.

## Lighting benchmark prototypes

Phase 7D adds an isolated development benchmark. It does not change the normal renderer path or
the public RHI. The benchmark uses the same procedural cube and generates point lights in a stable
golden-angle sequence. It evaluates four paths in a fixed order (`forward`, `forward_plus`,
`clustered`, `deferred`) for 1k, 10k and 100k instances combined with 1, 32 and 256 lights,
using 10 warmup frames and 30 measured frames per case. CPU culling is the default; passing
`--gpu-culling` selects the opt-in GPU culling path when compute resources are available.

The command is:

```text
gameengine_runtime --renderer-benchmark
gameengine_runtime --renderer-benchmark --gpu-culling
```

The benchmark records startup, pass CPU/GPU timing when timestamps are supported, draw/dispatch
counts, workload sizes and resource bytes. RAM and device-local heap data are reported as
`unavailable` when the platform or Vulkan implementation does not expose them. Output is written
to stdout and to the ignored local file
`build/renderer-benchmarks/lighting_benchmark_v1.txt`; no benchmark numbers are versioned.

The experimental benchmark graph remains isolated, while the production quality graph is:

```text
scene
  ↓
visibility mode
  ↓
CPU culling or gpu_cull
  ↓
optional shadow_depth / light_list_build
  ↓
forward_opaque
  ↓
Vulkan command buffer
```

Forward+ uses 16x16 tiles, Clustered uses 16x16x24 clusters, and Deferred keeps a minimal
G-buffer-shaped benchmark pass. The normal renderer selects Forward+ through the quality flag;
the benchmark still measures all four paths in its fixed order. The High profile uses only
procedural shadow/environment resources; no external model or texture is required. VRAM policy
and the final hardware baseline remain pending.
