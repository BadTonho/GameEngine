# Versioned Benchmark Workloads (v1)

`benchmarks/workloads_v1.json` defines the deterministic benchmark workloads and regression
thresholds used in Phase 10:

1. **Renderer profiles & culling**:
   - `gameengine_runtime --metrics --renderer-quality low`
   - `gameengine_runtime --metrics --renderer-quality medium`
   - `gameengine_runtime --metrics --renderer-quality high`
   - `gameengine_runtime --metrics --gpu-culling`
   - `gameengine_runtime --renderer-benchmark` (36 fixed cases across `forward`, `forward_plus`,
     `clustered`, and `deferred` for 1k/10k/100k instances and 1/32/256 lights)
2. **Prepared scene & asset loading workload (`prepared_scene_v1`)**:
   - Generated deterministically in `build/quality-reports/scene_v1/` using
     `gameengine-asset-tool` (`pack-mesh`, `pack-texture`, `pack-material`, `pack-scene`), then
     loaded via `gameengine_runtime --smoke-test --scene ... --assets-dir ...` to record
     `asset_load_ns` and `scene_load_ns`.
3. **Subsystem microbenchmarks** (enabled when `GAMEENGINE_BUILD_BENCHMARKS=ON`):
   - `gameengine_scene_benchmark`
   - `gameengine_animation_benchmark`
   - `gameengine_audio_benchmark`
   - `gameengine_physics_benchmark`
   - `gameengine_scripting_benchmark`
4. **Offline capture & regression check**:
   - Use `cargo run --manifest-path tools/Cargo.toml --bin gameengine-asset-tool -- quality-capture`
     and `quality-compare` to capture Release metrics + binary sizes into `build/quality-reports/`
     and fail on regressions exceeding `regression_thresholds_percent`.
   - Raw hardware numbers under `build/quality-reports/` and `build/renderer-benchmarks/` are
     local-only and ignored by git.
