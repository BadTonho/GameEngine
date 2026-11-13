# RHI Contract, Capabilities Matrix & Multi-Backend Architecture (Phase 11)

## RHI Boundary (`src/engine/rhi/`)

Scene, asset, physics, scripting, editor, and C ABI code interact exclusively with `gameengine::rhi::Renderer` (`src/engine/rhi/rhi.hpp`) and bridge helpers (`scene_bridge`, `editor::renderer_bridge`, `renderer::diagnostics`).

Internally, `rhi::Renderer` (`src/engine/rhi/rhi.cpp`) owns a `rhi::RendererBackend*` (`src/engine/rhi/rhi_backend.hpp`) selected via `RendererConfiguration::backend`:

- `BackendKind::auto_select`: resolves to `Metal` on Apple, `Vulkan` on Windows/Linux (or `D3D12` if Vulkan is disabled on Windows).
- `BackendKind::vulkan`: `VulkanBackend` (`src/engine/renderer/vulkan/renderer_vulkan.cpp`).
- `BackendKind::d3d12`: `D3D12Backend` (`src/engine/renderer/d3d12/renderer_d3d12.cpp`).
- `BackendKind::metal`: `MetalBackend` (`src/engine/renderer/metal/renderer_metal.mm` on Apple; `renderer_metal_stub.cpp` returning `ErrorCode::unsupported_platform` on non-Apple hosts).

## Backend Capabilities Matrix (`src/engine/rhi/rhi_capabilities.hpp`)

| Capability | Vulkan (`vulkan`) | Direct3D 12 (`d3d12`) | Metal (`metal`) |
| --- | --- | --- | --- |
| Shader Target | SPIR-V (`spirv_1_0+GLSL_450`) | DXIL (`sm_6_0`) | MSL (`metal`) |
| Compute Shaders | Supported | Supported | Supported |
| Indirect Draw | Supported | Supported | Supported |
| Multi-Draw Indirect | Supported | Supported | Disabled (fallback) |
| GPU Timestamps | Hardware-dependent | Supported | Supported |
| Shadow Map | Supported (High profile) | Supported (High profile) | Supported (High profile) |
| Environment Map | Supported (High profile) | Supported (High profile) | Supported (High profile) |
| Validation Layer | `VK_LAYER_KHRONOS_validation` | `ID3D12Debug` + DXGI Debug | Metal Validation |
| Max Frames in Flight | 2 | 2 | 2 |

## Shared Slang Shader Pipeline

All backends compile from the same `.slang` sources in `assets/shaders/bootstrap/` via `scripts/compile_bootstrap_shaders.cmake` and `scripts/compile_multi_backend_shaders.cmake`:

- **SPIR-V**: `src/engine/renderer/vulkan/triangle_shaders.hpp`
- **DXIL**: `src/engine/renderer/d3d12/d3d12_shaders.hpp`
- **Metal**: `src/engine/renderer/metal/metal_shaders.hpp`

Vertex/fragment stage structs carry both `[[vk::location(N)]]` (for SPIR-V) and HLSL semantics (`: POSITION`, `: NORMAL`, `: TEXCOORD0`..`4`, `: COLOR0`) so `slangc` emits valid SPIR-V, DXIL (`DXBC` validated), and Metal source from a single file.
