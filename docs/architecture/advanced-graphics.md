# Advanced Graphics Features (Phase 12)

Phase 12 introduces a modular, compile-time-removable advanced graphics layer in
`src/engine/renderer/advanced_graphics/` on top of `RenderGraph`, `RendererQuality`,
`BackendCapabilities`, and offline `Slang` shaders (`assets/shaders/advanced/*.slang`).

## Feature Matrix (Evaluation Order)

| # | Feature | CMake Option | Quality Tier | Hardware Gate | Fallback Target | Est. VRAM (1080p) |
|---|---------|--------------|--------------|---------------|-----------------|-------------------|
| 1 | TAA & Temporal Reconstruction | `GAMEENGINE_ENABLE_TAA` | Medium / High | Compute | `no_temporal_aa` | ~33.2 MB (`pixels * 16`) |
| 2 | Upscaling | `GAMEENGINE_ENABLE_UPSCALING` | Medium / High | Compute | `native_resolution` | ~16.6 MB (`pixels * 8`) |
| 3 | SSAO | `GAMEENGINE_ENABLE_SSAO` | Medium / High | Compute | `ambient_hemisphere_only` | ~4.1 MB (`pixels * 2`) |
| 4 | SSR | `GAMEENGINE_ENABLE_SSR` | High | Compute | `environment_cubemap_only` | ~4.1 MB (`(pixels/4)*8 + 4KB`) |
| 5 | Contact Shadows | `GAMEENGINE_ENABLE_CONTACT_SHADOWS` | Medium / High | Compute | `standard_shadow_map` | ~2.1 MB (`pixels * 1`) |
| 6 | Volumetric Fog | `GAMEENGINE_ENABLE_VOLUMETRIC_FOG` | High | Compute | `analytic_distance_fog` | ~14.7 MB (`160*90*64*16`) |
| 7 | GPU Particles | `GAMEENGINE_ENABLE_GPU_PARTICLES` | Medium / High | Compute | `cpu_or_disabled` | ~2.1 MB (`65536*32 + 64`) |
| 8 | Real-time GI | `GAMEENGINE_ENABLE_REALTIME_GI` | High | Compute | `baked_ambient_probes` | ~131 KB (`16^3 * 32`) |
| 9 | Ray Tracing | `GAMEENGINE_ENABLE_RAY_TRACING` | High | `ray_tracing_supported` | `contact_shadows_and_ssr` | ~20.8 MB (`pixels*8 + 4MB`) |
| 10 | Virtualized Geometry | `GAMEENGINE_ENABLE_VIRTUALIZED_GEOMETRY` | High | `mesh_shaders_supported` | `forward_plus_instanced_lods` | ~590 KB (`16384 * 36`) |

## Compile-Time Removal & Build-Size Impact

- Set `-DGAMEENGINE_MINIMAL_GRAPHICS=ON` to strip all 10 advanced graphics modules and their
  compiled SPIR-V shader arrays from `gameengine_renderer`.
- Measured locally on Windows x64 MSVC Release:
  - `gameengine_renderer.lib` (`GAMEENGINE_MINIMAL_GRAPHICS=ON`): `3,113,042` bytes
  - `gameengine_renderer.lib` (all 10 features `ON`): `3,196,722` bytes (`+83,680` bytes)
- Each feature can also be toggled individually via `-DGAMEENGINE_ENABLE_<FEATURE>=OFF`.
- Runtime overhead when disabled (`--no-advanced-graphics` or `low` quality): `0` passes, `0` VRAM, `0` ns.

## Runtime Telemetry & Verification

- Pass `--advanced-graphics` to `gameengine_runtime` to resolve features against live
  `BackendCapabilities` and `RendererQuality`, compile the acyclic `RenderGraph`, and log
  `active`, `fallbacks`, `passes`, `vram_bytes`, `ram_bytes`, `est_cpu_ns`, and `est_gpu_ns`.
- Unit & stability tests: `tests/advanced_graphics_tests.cpp` (`gameengine_advanced_graphics`).
