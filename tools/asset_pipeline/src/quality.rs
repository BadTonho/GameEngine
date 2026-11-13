use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use serde::{Deserialize, Serialize};

use crate::format;
use crate::model::{
    fixture_material, fixture_mesh, fixture_scene, fixture_texture, pack_material, pack_mesh,
    pack_scene, pack_texture, write_asset,
};
use crate::{AssetError, Result};

#[derive(Debug, Clone, Default, Serialize, Deserialize, PartialEq, Eq)]
pub struct WorkloadMetrics {
    pub startup_ns: Option<u64>,
    pub asset_load_ns: Option<u64>,
    pub scene_load_ns: Option<u64>,
    pub ram_current_bytes: Option<u64>,
    pub ram_peak_bytes: Option<u64>,
    pub vram_total_bytes: Option<u64>,
    pub vram_peak_bytes: Option<u64>,
    pub draw_calls: Option<u64>,
    pub cpu_avg_ns: Option<u64>,
    pub gpu_avg_ns: Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QualityReport {
    pub schema_version: u32,
    pub binary_sizes: BTreeMap<String, u64>,
    pub workloads: BTreeMap<String, WorkloadMetrics>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RegressionThresholds {
    pub binary_size: f64,
    pub vram_bytes: f64,
    pub ram_bytes: f64,
    pub startup_ns: f64,
    pub load_ns: f64,
    pub cpu_ns: f64,
    pub gpu_ns: f64,
}

impl Default for RegressionThresholds {
    fn default() -> Self {
        Self {
            binary_size: 10.0,
            vram_bytes: 15.0,
            ram_bytes: 15.0,
            startup_ns: 25.0,
            load_ns: 25.0,
            cpu_ns: 20.0,
            gpu_ns: 20.0,
        }
    }
}

fn parse_first_slash_u64(value: &str) -> Option<u64> {
    value.split('/').next()?.parse::<u64>().ok()
}

pub fn parse_metrics_output(text: &str) -> WorkloadMetrics {
    let mut metrics = WorkloadMetrics::default();
    for line in text.lines() {
        for token in line.split_whitespace() {
            let Some((key, value)) = token.split_once('=') else {
                continue;
            };
            match key {
                "startup_ns" => metrics.startup_ns = value.parse().ok(),
                "asset_load_ns" => metrics.asset_load_ns = value.parse().ok(),
                "scene_load_ns" => metrics.scene_load_ns = value.parse().ok(),
                "ram_current" => metrics.ram_current_bytes = value.parse().ok(),
                "ram_peak" => metrics.ram_peak_bytes = value.parse().ok(),
                "vram_total_bytes" => metrics.vram_total_bytes = value.parse().ok(),
                "vram_peak_bytes" => metrics.vram_peak_bytes = value.parse().ok(),
                "draw_calls" => metrics.draw_calls = value.parse().ok(),
                "cpu_ns(avg/min/max)" => metrics.cpu_avg_ns = parse_first_slash_u64(value),
                "gpu_ns(avg/min/max)" => metrics.gpu_avg_ns = parse_first_slash_u64(value),
                _ => {}
            }
        }
    }
    metrics
}

fn find_artifact(build_dir: &Path, name: &str) -> Option<PathBuf> {
    let candidates = [
        build_dir.join(name),
        build_dir.join("Release").join(name),
        build_dir.join("Debug").join(name),
    ];
    candidates.into_iter().find(|path| path.is_file())
}

fn run_runtime(runtime_exe: &Path, args: &[&str]) -> Result<WorkloadMetrics> {
    let output = Command::new(runtime_exe).args(args).output()?;
    let combined = format!(
        "{}\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );
    if !output.status.success() {
        return Err(AssetError::Invalid(format!(
            "runtime command failed ({args:?}): {combined}"
        )));
    }
    Ok(parse_metrics_output(&combined))
}

pub fn generate_prepared_scene_v1(scene_dir: &Path) -> Result<PathBuf> {
    fs::create_dir_all(scene_dir)?;
    let tex_bytes = pack_texture(&fixture_texture())?;
    let tex_id = format::parse(&tex_bytes)?.asset_id;
    let mesh_bytes = pack_mesh(&fixture_mesh())?;
    let mesh_id = format::parse(&mesh_bytes)?.asset_id;
    let mat_bytes = pack_material(&fixture_material(tex_id))?;
    let mat_id = format::parse(&mat_bytes)?.asset_id;
    let scene_bytes = pack_scene(&fixture_scene(mesh_id, mat_id))?;

    write_asset(&scene_dir.join("fixture.getex"), &tex_bytes)?;
    write_asset(&scene_dir.join("fixture.gemesh"), &mesh_bytes)?;
    write_asset(&scene_dir.join("fixture.gemat"), &mat_bytes)?;
    let scene_path = scene_dir.join("fixture.gescene");
    write_asset(&scene_path, &scene_bytes)?;
    Ok(scene_path)
}

pub fn capture_quality_report(
    build_dir: &Path,
    work_dir: &Path,
    output_path: &Path,
) -> Result<QualityReport> {
    let mut binary_sizes = BTreeMap::new();
    for target in [
        "gameengine_runtime.exe",
        "gameengine_editor.exe",
        "gameengine_c_api.lib",
        "gameengine_runtime",
        "gameengine_editor",
        "libgameengine_c_api.a",
    ] {
        if let Some(path) = find_artifact(build_dir, target) {
            binary_sizes.insert(target.to_string(), fs::metadata(path)?.len());
        }
    }

    let runtime_exe = find_artifact(build_dir, "gameengine_runtime.exe")
        .or_else(|| find_artifact(build_dir, "gameengine_runtime"))
        .ok_or_else(|| AssetError::Invalid("gameengine_runtime binary not found".into()))?;

    let mut workloads = BTreeMap::new();
    let scene_dir = work_dir.join("scene_v1");
    let scene_path = generate_prepared_scene_v1(&scene_dir)?;
    let scene_str = scene_path.to_string_lossy().into_owned();
    let dir_str = scene_dir.to_string_lossy().into_owned();
    workloads.insert(
        "prepared_scene_v1".into(),
        run_runtime(
            &runtime_exe,
            &[
                "--smoke-test",
                "--scene",
                &scene_str,
                "--assets-dir",
                &dir_str,
            ],
        )?,
    );

    for (name, args) in [
        (
            "renderer_low_cpu",
            &["--metrics", "--renderer-quality", "low"][..],
        ),
        (
            "renderer_medium_cpu",
            &["--metrics", "--renderer-quality", "medium"][..],
        ),
        (
            "renderer_high_cpu",
            &["--metrics", "--renderer-quality", "high"][..],
        ),
        ("renderer_medium_gpu", &["--metrics", "--gpu-culling"][..]),
    ] {
        workloads.insert(name.into(), run_runtime(&runtime_exe, args)?);
    }

    let report = QualityReport {
        schema_version: 1,
        binary_sizes,
        workloads,
    };
    if let Some(parent) = output_path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(output_path, serde_json::to_string_pretty(&report)?)?;
    Ok(report)
}

fn check_metric(
    label: &str,
    baseline: Option<u64>,
    candidate: Option<u64>,
    max_increase_percent: f64,
    failures: &mut Vec<String>,
) {
    let (Some(base), Some(cand)) = (baseline, candidate) else {
        return;
    };
    if base == 0 || cand <= base {
        return;
    }
    let increase_percent = ((cand - base) as f64 / base as f64) * 100.0;
    if increase_percent > max_increase_percent {
        failures.push(format!(
            "{label}: baseline={base} candidate={cand} (+{increase_percent:.2}% > +{max_increase_percent:.2}%)"
        ));
    }
}

pub fn compare_quality_reports(
    baseline: &QualityReport,
    candidate: &QualityReport,
    thresholds: &RegressionThresholds,
) -> std::result::Result<(), Vec<String>> {
    let mut failures = Vec::new();
    for (name, base_size) in &baseline.binary_sizes {
        let cand_size = candidate.binary_sizes.get(name).copied();
        check_metric(
            &format!("binary_size.{name}"),
            Some(*base_size),
            cand_size,
            thresholds.binary_size,
            &mut failures,
        );
    }
    for (name, base) in &baseline.workloads {
        let Some(cand) = candidate.workloads.get(name) else {
            failures.push(format!("workload.{name}: missing in candidate report"));
            continue;
        };
        check_metric(
            &format!("{name}.startup_ns"),
            base.startup_ns,
            cand.startup_ns,
            thresholds.startup_ns,
            &mut failures,
        );
        check_metric(
            &format!("{name}.asset_load_ns"),
            base.asset_load_ns,
            cand.asset_load_ns,
            thresholds.load_ns,
            &mut failures,
        );
        check_metric(
            &format!("{name}.scene_load_ns"),
            base.scene_load_ns,
            cand.scene_load_ns,
            thresholds.load_ns,
            &mut failures,
        );
        check_metric(
            &format!("{name}.ram_peak_bytes"),
            base.ram_peak_bytes,
            cand.ram_peak_bytes,
            thresholds.ram_bytes,
            &mut failures,
        );
        check_metric(
            &format!("{name}.vram_total_bytes"),
            base.vram_total_bytes,
            cand.vram_total_bytes,
            thresholds.vram_bytes,
            &mut failures,
        );
        check_metric(
            &format!("{name}.vram_peak_bytes"),
            base.vram_peak_bytes,
            cand.vram_peak_bytes,
            thresholds.vram_bytes,
            &mut failures,
        );
        check_metric(
            &format!("{name}.cpu_avg_ns"),
            base.cpu_avg_ns,
            cand.cpu_avg_ns,
            thresholds.cpu_ns,
            &mut failures,
        );
        check_metric(
            &format!("{name}.gpu_avg_ns"),
            base.gpu_avg_ns,
            cand.gpu_avg_ns,
            thresholds.gpu_ns,
            &mut failures,
        );
    }
    if failures.is_empty() {
        Ok(())
    } else {
        Err(failures)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_runtime_metrics_and_scene_load_telemetry() {
        let sample = "\
[gameengine] [info] scene_load_telemetry asset_load_ns=120000 scene_load_ns=340000 assets=4 entities=3
[gameengine] [info] renderer metrics: mode=cpu gpu_culling=available quality=low->low instances=100000 frames=30 draw_calls=30 visible_avg=8661 culled_avg=91339 vram_total_bytes=16749520 vram_peak_bytes=28872912
[gameengine] [info] pass=forward_opaque cpu_ns(avg/min/max)=33046/14700/64500 draw_calls_avg=1 gpu_ns(avg/min/max)=186091/175424/196608
";
        let parsed = parse_metrics_output(sample);
        assert_eq!(parsed.asset_load_ns, Some(120_000));
        assert_eq!(parsed.scene_load_ns, Some(340_000));
        assert_eq!(parsed.vram_total_bytes, Some(16_749_520));
        assert_eq!(parsed.vram_peak_bytes, Some(28_872_912));
        assert_eq!(parsed.draw_calls, Some(30));
        assert_eq!(parsed.cpu_avg_ns, Some(33_046));
        assert_eq!(parsed.gpu_avg_ns, Some(186_091));
    }

    #[test]
    fn regression_checker_passes_within_threshold_and_flags_excess() {
        let mut baseline = QualityReport {
            schema_version: 1,
            binary_sizes: BTreeMap::from([("gameengine_runtime.exe".into(), 1_000_000)]),
            workloads: BTreeMap::from([(
                "renderer_low_cpu".into(),
                WorkloadMetrics {
                    vram_total_bytes: Some(10_000_000),
                    gpu_avg_ns: Some(200_000),
                    ..WorkloadMetrics::default()
                },
            )]),
        };
        let within = QualityReport {
            schema_version: 1,
            binary_sizes: BTreeMap::from([("gameengine_runtime.exe".into(), 1_050_000)]),
            workloads: BTreeMap::from([(
                "renderer_low_cpu".into(),
                WorkloadMetrics {
                    vram_total_bytes: Some(11_000_000),
                    gpu_avg_ns: Some(220_000),
                    ..WorkloadMetrics::default()
                },
            )]),
        };
        assert!(
            compare_quality_reports(&baseline, &within, &RegressionThresholds::default()).is_ok()
        );

        baseline
            .workloads
            .get_mut("renderer_low_cpu")
            .expect("present")
            .gpu_avg_ns = Some(100_000);
        let err = compare_quality_reports(&baseline, &within, &RegressionThresholds::default())
            .expect_err("120% GPU increase should fail");
        assert!(err[0].contains("renderer_low_cpu.gpu_avg_ns"));
    }

    #[test]
    fn deterministic_seeded_fuzzing_on_container_parser_never_panics() {
        let mut state: u64 = 0x9e37_79b9_7f4a_7c15;
        let mut next_u64 = || {
            state = state.wrapping_add(0x9e37_79b9_7f4a_7c15);
            let mut z = state;
            z = (z ^ (z >> 30)).wrapping_mul(0xbf58_476d_1ce4_e5b9);
            z = (z ^ (z >> 27)).wrapping_mul(0x94d0_49bb_1331_11eb);
            z ^ (z >> 31)
        };
        let seeds = [
            pack_mesh(&fixture_mesh()).expect("mesh"),
            pack_texture(&fixture_texture()).expect("texture"),
            pack_material(&fixture_material(0)).expect("material"),
            pack_scene(&fixture_scene(1, 2)).expect("scene"),
        ];
        for seed in &seeds {
            for _ in 0..256 {
                let mut mutated = seed.clone();
                let op = next_u64() % 3;
                if op == 0 && !mutated.is_empty() {
                    let new_len = (next_u64() as usize) % mutated.len();
                    mutated.truncate(new_len);
                } else if !mutated.is_empty() {
                    let idx = (next_u64() as usize) % mutated.len();
                    mutated[idx] ^= (next_u64() & 0xff) as u8;
                }
                let _ = format::parse(&mutated);
            }
        }
    }
}
