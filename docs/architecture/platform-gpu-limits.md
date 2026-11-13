# Platform, GPU Limitations & Continuous Quality (Phase 10)

## Platform Support Matrix

- **Windows (x64, MSVC)**: Primary verified host. Win32 windowing, WASAPI audio, Vulkan 1.0+
  rendering, native D3D12 (`GAMEENGINE_BUILD_D3D12=ON`, `--renderer-backend d3d12`), Jolt physics,
  optional Lua 5.4.8 scripting, and C11 C ABI are supported and tested locally
  (`windows-msvc-debug`, `windows-msvc-release`, `windows-msvc-asan`, `windows-msvc-analyze`).
- **Linux (GCC / Clang)**: Supported via X11 windowing, ALSA audio, Vulkan 1.0+, and
  `linux-gcc-sanitizers` (ASan + UBSan). Not executed directly on Windows hosts.
- **macOS / Apple targets**: Metal backend (`src/engine/renderer/metal/renderer_metal.mm`,
  `GAMEENGINE_BUILD_METAL=ON` on Apple) is isolated behind the `RendererBackend` RHI boundary;
  shared `.slang` bootstrap shaders are validated offline to Metal (`metal_shaders.hpp`) and DXIL
  (`d3d12_shaders.hpp`) alongside SPIR-V (`triangle_shaders.hpp`). On non-Apple hosts,
  `renderer_metal_stub.cpp` returns `ErrorCode::unsupported_platform`. See
  [rhi-backends.md](rhi-backends.md) for the full backend capabilities matrix.

## GPU & Vulkan Limitations

- **Vulkan 1.0 baseline**: Core rendering, compute culling (`VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` /
  `INDIRECT_BUFFER_BIT`), Forward+ tile lists, and High shadow/environment passes target Vulkan 1.0
  without requiring ray tracing, mesh shaders, or timeline semaphores.
- **GPU timestamps**: Enabled when `VkPhysicalDeviceLimits::timestampPeriod > 0` and the graphics
  queue reports `timestampValidBits > 0`; otherwise GPU timings fall back to `0` (`unavailable`).
- **VRAM budget & heaps**: Tracked via `allocate_tracked_memory` / `free_tracked_memory`. Default
  budget caps at `min(device_local_heap_bytes, 512 MiB)` (or `256 MiB` if no device-local heap
  is exposed).
- **Quality profile fallbacks**:
  - `Low`: directional light only (`light_bytes = 0`, `shadow_map_bytes = 0`, `environment_bytes = 0`).
  - `Medium` (default): Forward+ 16x16 tile lighting; falls back to `Low` if compute resources fail.
  - `High`: adds `shadow_depth` + procedural cubemap; falls back to `Medium` or analytic environment
    when shadow/cubemap allocation fails.
- **Validation & debug markers**: `VK_LAYER_KHRONOS_validation`, `VkDebugUtilsMessengerEXT`,
  `vkSetDebugUtilsObjectNameEXT`, and `vkCmdBeginDebugUtilsLabelEXT`/`End` are enabled when
  available in Debug/validation configurations.

## Local Quality, Sanitizer & Regression Protocol

Per `AGENTS.md` Rule 23, all verification runs locally (GitHub Actions is not used):

1. **Before/after measurements**:
   - Capture Release metrics and binary sizes:
     `cargo run --manifest-path tools/Cargo.toml --bin gameengine-asset-tool -- quality-capture --build-dir build/windows-msvc-release --output build/quality-reports/baseline.json`
   - Compare candidate builds against baseline:
     `cargo run --manifest-path tools/Cargo.toml --bin gameengine-asset-tool -- quality-compare --baseline build/quality-reports/baseline.json --candidate build/quality-reports/latest.json`
2. **Sanitizers & static analysis**:
   - Windows ASan: `cmake --preset windows-msvc-asan && cmake --build --preset windows-msvc-asan && ctest --preset windows-msvc-asan --output-on-failure`
   - Windows static analysis (`/analyze`): `cmake --preset windows-msvc-analyze && cmake --build --preset windows-msvc-analyze`
   - Linux ASan/UBSan: `cmake --preset linux-gcc-sanitizers && cmake --build --preset linux-gcc-sanitizers && ctest --preset linux-gcc-sanitizers`
3. **Hardening suite**: `gameengine_hardening` (`tests/hardening_tests.cpp`) and `gameengine_vulkan_resize`
   verify repeated init/shutdown cycles, seeded binary parser fuzzing, and stale-handle rejection.
