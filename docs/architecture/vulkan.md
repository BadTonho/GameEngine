# Vulkan renderer foundation

Phase 2 adds the first renderer path without exposing Vulkan types to shared engine headers.

## Ownership

`gameengine_renderer` owns the Vulkan instance, debug messenger, surface, physical-device selection, logical device, queues, command pool, swapchain, image views, render pass, pipeline, framebuffers, command buffers and synchronization objects.

The renderer is non-copyable. Its `shutdown()` waits for the device only during explicit destruction, releases swapchain resources before the device, destroys the surface before the instance and destroys the validation messenger before the instance. No renderer resource is destroyed from the normal per-frame path.

Buffers, images, samplers and pipelines are addressed by typed index-plus-generation handles. A destroyed handle becomes invalid immediately; its Vulkan object remains pending until the frame fence associated with the destruction has signaled. Resource memory uses direct `VkDeviceMemory`; each resource owns its allocation in this phase and no custom allocator or VMA dependency is used.

The platform owns the X11 display and window. It exposes only opaque native handles through `NativeWindowHandles`; the Vulkan backend translates those handles into `VkXlibSurfaceCreateInfoKHR`.

## Frame lifecycle

Two frames in flight are used. A frame waits for its fence, acquires a swapchain image, records a command buffer, submits it to the graphics queue and presents it on the presentation queue.

`VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` trigger explicit swapchain recreation. A zero-sized window skips rendering until it has a valid extent. `VK_ERROR_SURFACE_LOST_KHR` returns a status to the runtime, which performs an orderly renderer shutdown.

The integration test resizes the X11 window, pumps the resulting configure event and renders again through the recreated swapchain.

Vertex and image uploads use temporary host-visible, coherent staging buffers and one-time command buffers. Completion waits on a dedicated fence, never on `vkDeviceWaitIdle` during normal frame submission. The triangle uses a device-local vertex buffer with position and RGB color attributes. Images and samplers are created and uploaded by integration tests but are not bound to the triangle until a later phase.

## Validation and shaders

Debug builds require `VK_LAYER_KHRONOS_validation` and enable `VK_EXT_debug_utils`. Validation errors are logged and cause the smoke test to fail. Release builds do not require validation layers. Debug object names and command labels are provided through the same extension without a RenderDoc dependency.

The triangle uses precompiled SPIR-V generated from `assets/shaders/bootstrap/triangle.slang` by `scripts/compile_bootstrap_shaders.cmake`. The offline target requires the pinned `slangc` version `2026.13.1-1-g84792eb15`, emits reflection JSON and generates the checked-in `src/engine/renderer/vulkan/triangle_shaders.hpp` header. The compiler is never a runtime dependency.

The bootstrap shader has explicit `vertex_main` and `fragment_main` entry points. Vertex position and color use Vulkan locations 0 and 1, and the fragment color uses location 0. The compiler uses the Vulkan 1.0-compatible SPIR-V 1.0 profile together with the `-emit-spirv-via-glsl` path because direct emission from the pinned Slang build requires a newer SPIR-V profile and can emit `SPV_GOOGLE_hlsl_functionality1`, which is not enabled by the initial Vulkan device configuration.

To regenerate the artifacts:

```text
cmake --build build/<preset> --target gameengine_compile_bootstrap_shaders --config Debug
```

The normal runtime build consumes only the generated header. Shader caches, variants, runtime reflection and hot reload remain outside this bootstrap milestone.
