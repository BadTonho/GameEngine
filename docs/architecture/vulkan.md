# Vulkan renderer foundation

Phase 2A adds the first renderer path without exposing Vulkan types to shared engine headers.

## Ownership

`gameengine_renderer` owns the Vulkan instance, debug messenger, surface, physical-device selection, logical device, queues, command pool, swapchain, image views, render pass, pipeline, framebuffers, command buffers and synchronization objects.

The renderer is non-copyable. Its `shutdown()` waits for the device only during explicit destruction, releases swapchain resources before the device, destroys the surface before the instance and destroys the validation messenger before the instance. No renderer resource is destroyed from the normal per-frame path.

The platform owns the X11 display and window. It exposes only opaque native handles through `NativeWindowHandles`; the Vulkan backend translates those handles into `VkXlibSurfaceCreateInfoKHR`.

## Frame lifecycle

Two frames in flight are used. A frame waits for its fence, acquires a swapchain image, records a command buffer, submits it to the graphics queue and presents it on the presentation queue.

`VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` trigger explicit swapchain recreation. A zero-sized window skips rendering until it has a valid extent. `VK_ERROR_SURFACE_LOST_KHR` returns a status to the runtime, which performs an orderly renderer shutdown.

The integration test resizes the X11 window, pumps the resulting configure event and renders again through the recreated swapchain.

## Validation and shaders

Debug builds require `VK_LAYER_KHRONOS_validation` and enable `VK_EXT_debug_utils`. Validation errors are logged and cause the smoke test to fail. Release builds do not require validation layers.

The triangle uses precompiled SPIR-V generated from the bootstrap GLSL sources by `scripts/generate_bootstrap_shaders.sh`. The compiler is an offline development tool and is not a runtime dependency. Slang remains the source language for the complete shader pipeline in Phase 3.
