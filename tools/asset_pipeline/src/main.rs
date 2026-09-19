use std::collections::HashSet;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

use gameengine_asset_tool::format::{self, AssetKind, ChunkInput};
use gameengine_asset_tool::gltf::{import_gltf, write_imported_assets, ImportedAsset};
use gameengine_asset_tool::model::{
    fixture_material, fixture_mesh, fixture_scene, fixture_texture, pack_material, pack_mesh,
    pack_scene, pack_texture, write_asset,
};
use gameengine_asset_tool::{AssetError, Result};

struct PackageAsset {
    asset_id: u64,
    content_hash: u64,
    magic: [u8; 4],
    path: PathBuf,
    normalized_name: String,
    bytes: Vec<u8>,
}

fn usage() {
    eprintln!(
        "usage:\n  asset-tool validate <asset>\n  asset-tool inspect <asset>\n  asset-tool import-gltf <input.gltf> --output-dir <dir>\n  asset-tool pack-mesh [output]\n  asset-tool pack-texture [output]\n  asset-tool pack-material [output]\n  asset-tool pack-scene [output]\n  asset-tool package --input-dir <dir> --output <package>"
    );
}

fn require_path(value: Option<&String>, name: &str) -> Result<PathBuf> {
    value
        .map(PathBuf::from)
        .ok_or_else(|| AssetError::Invalid(format!("missing {name}")))
}

fn serialize_imported_assets(assets: &[ImportedAsset]) -> Result<Vec<u8>> {
    let mut output = b"GIC1".to_vec();
    output.extend_from_slice(&(assets.len() as u32).to_le_bytes());
    for asset in assets {
        let name = asset.name.as_bytes();
        let name_length = u32::try_from(name.len())
            .map_err(|_| AssetError::Invalid("cached asset name is too long".into()))?;
        let byte_length = u32::try_from(asset.bytes.len())
            .map_err(|_| AssetError::Invalid("cached asset is too large".into()))?;
        output.extend_from_slice(&name_length.to_le_bytes());
        output.extend_from_slice(name);
        output.extend_from_slice(&byte_length.to_le_bytes());
        output.extend_from_slice(&asset.bytes);
    }
    Ok(output)
}

fn deserialize_imported_assets(bytes: &[u8]) -> Result<Vec<ImportedAsset>> {
    if bytes.len() < 8 || &bytes[0..4] != b"GIC1" {
        return Err(AssetError::Invalid("asset cache header is invalid".into()));
    }
    let mut cursor = 4_usize;
    let read_u32 = |bytes: &[u8], cursor: &mut usize| -> Result<u32> {
        let end = cursor
            .checked_add(4)
            .ok_or_else(|| AssetError::Invalid("asset cache offset overflow".into()))?;
        let value = bytes
            .get(*cursor..end)
            .ok_or_else(|| AssetError::Invalid("asset cache is truncated".into()))?;
        *cursor = end;
        Ok(u32::from_le_bytes(
            value.try_into().expect("checked length"),
        ))
    };
    let count = read_u32(bytes, &mut cursor)? as usize;
    if count > format::MAX_CHUNKS {
        return Err(AssetError::Invalid(
            "asset cache has too many outputs".into(),
        ));
    }
    let mut assets = Vec::with_capacity(count);
    for _ in 0..count {
        let name_length = read_u32(bytes, &mut cursor)? as usize;
        let name_end = cursor
            .checked_add(name_length)
            .ok_or_else(|| AssetError::Invalid("asset cache name overflows".into()))?;
        let name = String::from_utf8(
            bytes
                .get(cursor..name_end)
                .ok_or_else(|| AssetError::Invalid("asset cache name is truncated".into()))?
                .to_vec(),
        )
        .map_err(|_| AssetError::Invalid("asset cache name is not UTF-8".into()))?;
        cursor = name_end;
        let byte_length = read_u32(bytes, &mut cursor)? as usize;
        let bytes_end = cursor
            .checked_add(byte_length)
            .ok_or_else(|| AssetError::Invalid("asset cache payload overflows".into()))?;
        let payload = bytes
            .get(cursor..bytes_end)
            .ok_or_else(|| AssetError::Invalid("asset cache payload is truncated".into()))?
            .to_vec();
        cursor = bytes_end;
        let kind = format::parse(&payload)?.kind;
        assets.push(ImportedAsset {
            name,
            kind,
            bytes: payload,
        });
    }
    if cursor != bytes.len() {
        return Err(AssetError::Invalid("asset cache has trailing bytes".into()));
    }
    Ok(assets)
}

fn validate_semantics(bytes: &[u8]) -> Result<()> {
    let parsed = format::parse(bytes)?;
    match parsed.kind {
        AssetKind::Mesh => {
            let metadata = parsed
                .chunk(format::CHUNK_MESH_HEADER)
                .ok_or_else(|| AssetError::Invalid("mesh header chunk is missing".into()))?;
            if metadata.data.len() != 24 {
                return Err(AssetError::Invalid(
                    "mesh header chunk has an invalid size".into(),
                ));
            }
            let vertices = parsed
                .chunk(format::CHUNK_VERTICES)
                .ok_or_else(|| AssetError::Invalid("mesh vertex chunk is missing".into()))?;
            let indices = parsed
                .chunk(format::CHUNK_INDICES)
                .ok_or_else(|| AssetError::Invalid("mesh index chunk is missing".into()))?;
            if metadata.data[0..4] != 1_u32.to_le_bytes()
                || metadata.data[8..12] != 32_u32.to_le_bytes()
            {
                return Err(AssetError::Invalid(
                    "mesh vertex layout is unsupported".into(),
                ));
            }
            let vertex_count = format::read_u32_public(metadata.data, 4)? as usize;
            let index_format = format::read_u32_public(metadata.data, 12)?;
            let index_count = format::read_u32_public(metadata.data, 16)? as usize;
            let vertex_bytes = vertex_count
                .checked_mul(32)
                .ok_or_else(|| AssetError::Invalid("mesh vertex count overflows".into()))?;
            let index_width = match index_format {
                16 => 2,
                32 => 4,
                _ => {
                    return Err(AssetError::Invalid(
                        "mesh index format is unsupported".into(),
                    ))
                }
            };
            if vertices.data.len() != vertex_bytes
                || indices.data.len() != index_count * index_width
            {
                return Err(AssetError::Invalid(
                    "mesh payload size does not match metadata".into(),
                ));
            }
        }
        AssetKind::Texture => {
            let metadata = parsed
                .chunk(format::CHUNK_TEXTURE_HEADER)
                .ok_or_else(|| AssetError::Invalid("texture header chunk is missing".into()))?;
            let pixels = parsed
                .chunk(format::CHUNK_TEXTURE_DATA)
                .ok_or_else(|| AssetError::Invalid("texture data chunk is missing".into()))?;
            if metadata.data.len() != 20 || metadata.data[12..16] != 1_u32.to_le_bytes() {
                return Err(AssetError::Invalid("texture header is unsupported".into()));
            }
            let width = format::read_u32_public(metadata.data, 0)? as usize;
            let height = format::read_u32_public(metadata.data, 4)? as usize;
            let expected = width
                .checked_mul(height)
                .and_then(|count| count.checked_mul(4))
                .ok_or_else(|| AssetError::Invalid("texture dimensions overflow".into()))?;
            if pixels.data.len() != expected {
                return Err(AssetError::Invalid(
                    "texture data size does not match metadata".into(),
                ));
            }
        }
        AssetKind::Material => {
            let metadata = parsed
                .chunk(format::CHUNK_MATERIAL_HEADER)
                .ok_or_else(|| AssetError::Invalid("material header chunk is missing".into()))?;
            if metadata.data.len() != 64 {
                return Err(AssetError::Invalid(
                    "material header has an invalid size".into(),
                ));
            }
        }
        AssetKind::Scene => {
            let metadata = parsed
                .chunk(format::CHUNK_SCENE_HEADER)
                .ok_or_else(|| AssetError::Invalid("scene header chunk is missing".into()))?;
            let count = format::read_u32_public(metadata.data, 0)? as usize;
            if count == 0 || count > 1_000_000 {
                return Err(AssetError::Invalid("scene entity count is invalid".into()));
            }
            if metadata.data.len() == 8 {
                let instances = parsed.chunk(format::CHUNK_INSTANCES).ok_or_else(|| {
                    AssetError::Invalid("scene instances chunk is missing".into())
                })?;
                if instances.data.len() != count * 80 {
                    return Err(AssetError::Invalid(
                        "scene instance data does not match metadata".into(),
                    ));
                }
            } else if metadata.data.len() == 16 {
                let entities = parsed
                    .chunk(format::CHUNK_SCENE_ENTITIES)
                    .ok_or_else(|| AssetError::Invalid("scene entity chunk is missing".into()))?;
                let transforms = parsed
                    .chunk(format::CHUNK_SCENE_TRANSFORMS)
                    .ok_or_else(|| {
                        AssetError::Invalid("scene transform chunk is missing".into())
                    })?;
                let mesh_renderers = parsed
                    .chunk(format::CHUNK_SCENE_MESH_RENDERERS)
                    .ok_or_else(|| {
                        AssetError::Invalid("scene mesh renderer chunk is missing".into())
                    })?;
                if entities.data.len() != count * 16
                    || transforms.data.len() != count * 48
                    || mesh_renderers.data.len() % 24 != 0
                {
                    return Err(AssetError::Invalid(
                        "scene graph data does not match metadata".into(),
                    ));
                }
                let ids: Vec<u64> = (0..count)
                    .map(|index| format::read_u64_public(entities.data, index * 16))
                    .collect::<Result<Vec<_>>>()?;
                if ids.contains(&0) || ids.windows(2).any(|pair| pair[0] == pair[1]) {
                    return Err(AssetError::Invalid("scene entity IDs are invalid".into()));
                }
                for index in 0..count {
                    let parent = format::read_u64_public(entities.data, index * 16 + 8)?;
                    if parent != 0 && !ids.contains(&parent) {
                        return Err(AssetError::Invalid(
                            "scene parent reference is missing".into(),
                        ));
                    }
                }
                for start in 0..count {
                    let mut current = start;
                    let mut reached_root = false;
                    for _ in 0..count {
                        let parent = format::read_u64_public(entities.data, current * 16 + 8)?;
                        if parent == 0 {
                            reached_root = true;
                            break;
                        }
                        current = ids.iter().position(|id| *id == parent).ok_or_else(|| {
                            AssetError::Invalid("scene parent reference is missing".into())
                        })?;
                    }
                    if !reached_root {
                        return Err(AssetError::Invalid(
                            "scene hierarchy contains a cycle".into(),
                        ));
                    }
                }
            } else {
                return Err(AssetError::Invalid(
                    "scene header has an invalid size".into(),
                ));
            }
        }
        AssetKind::Package => {}
    }
    Ok(())
}

fn contains_asset_id(ids: &[u64], id: u64) -> bool {
    ids.contains(&id)
}

fn validate_package_references(assets: &[PackageAsset]) -> Result<()> {
    let ids: Vec<u64> = assets.iter().map(|asset| asset.asset_id).collect();
    for asset in assets {
        let parsed = format::parse(&asset.bytes)?;
        match parsed.kind {
            AssetKind::Material => {
                let metadata = parsed.chunk(format::CHUNK_MATERIAL_HEADER).ok_or_else(|| {
                    AssetError::Invalid("material header chunk is missing".into())
                })?;
                for offset in [24, 32, 40] {
                    let id = format::read_u64_public(metadata.data, offset)?;
                    if id != 0 && !contains_asset_id(&ids, id) {
                        return Err(AssetError::Invalid(format!(
                            "{} references missing texture asset {id:016x}",
                            asset.path.display()
                        )));
                    }
                }
            }
            AssetKind::Scene => {
                let metadata = parsed
                    .chunk(format::CHUNK_SCENE_HEADER)
                    .ok_or_else(|| AssetError::Invalid("scene header chunk is missing".into()))?;
                if metadata.data.len() == 8 {
                    let instances = parsed.chunk(format::CHUNK_INSTANCES).ok_or_else(|| {
                        AssetError::Invalid("scene instances chunk is missing".into())
                    })?;
                    let count = format::read_u32_public(metadata.data, 0)? as usize;
                    for index in 0..count {
                        let offset = index * 80;
                        let mesh_id = format::read_u64_public(instances.data, offset + 64)?;
                        let material_id = format::read_u64_public(instances.data, offset + 72)?;
                        if !contains_asset_id(&ids, mesh_id) {
                            return Err(AssetError::Invalid(format!(
                                "{} references missing mesh asset {mesh_id:016x}",
                                asset.path.display()
                            )));
                        }
                        if !contains_asset_id(&ids, material_id) {
                            return Err(AssetError::Invalid(format!(
                                "{} references missing material asset {material_id:016x}",
                                asset.path.display()
                            )));
                        }
                    }
                } else if metadata.data.len() == 16 {
                    let mesh_renderers = parsed
                        .chunk(format::CHUNK_SCENE_MESH_RENDERERS)
                        .ok_or_else(|| {
                            AssetError::Invalid("scene mesh renderer chunk is missing".into())
                        })?;
                    for chunk in mesh_renderers.data.chunks_exact(24) {
                        let mesh_id = format::read_u64_public(chunk, 8)?;
                        let material_id = format::read_u64_public(chunk, 16)?;
                        if !contains_asset_id(&ids, mesh_id) {
                            return Err(AssetError::Invalid(format!(
                                "{} references missing mesh asset {mesh_id:016x}",
                                asset.path.display()
                            )));
                        }
                        if !contains_asset_id(&ids, material_id) {
                            return Err(AssetError::Invalid(format!(
                                "{} references missing material asset {material_id:016x}",
                                asset.path.display()
                            )));
                        }
                    }
                } else {
                    return Err(AssetError::Invalid(
                        "scene header has an invalid size".into(),
                    ));
                }
            }
            _ => {}
        }
    }
    Ok(())
}

fn fixture_output(command: &str, output: Option<&String>) -> Result<()> {
    let output = output.map_or_else(
        || PathBuf::from(format!("fixture.{}", command.trim_start_matches("pack-"))),
        PathBuf::from,
    );
    let bytes = match command {
        "pack-mesh" => pack_mesh(&fixture_mesh())?,
        "pack-texture" => pack_texture(&fixture_texture())?,
        "pack-material" => pack_material(&fixture_material(0))?,
        "pack-scene" => pack_scene(&fixture_scene(0, 0))?,
        _ => return Err(AssetError::Invalid("unknown fixture command".into())),
    };
    if let Some(parent) = output.parent().filter(|path| !path.as_os_str().is_empty()) {
        fs::create_dir_all(parent)?;
    }
    write_asset(&output, &bytes)
}

fn normalize_asset_name(path: &Path) -> Result<String> {
    let name = path
        .file_name()
        .and_then(|value| value.to_str())
        .ok_or_else(|| AssetError::Invalid("asset path is not valid UTF-8".into()))?;
    if name.is_empty() {
        return Err(AssetError::Invalid("asset path has an empty name".into()));
    }
    Ok(name.replace('\\', "/").to_ascii_lowercase())
}

fn package_assets(input_dir: &Path, output: &Path) -> Result<()> {
    let mut assets = Vec::new();
    for entry in fs::read_dir(input_dir)? {
        let entry = entry?;
        if !entry.file_type()?.is_file() {
            continue;
        }
        let path = entry.path();
        let normalized_name = normalize_asset_name(&path)?;
        let extension = path
            .extension()
            .and_then(|value| value.to_str())
            .unwrap_or_default();
        if !matches!(extension, "gemesh" | "getex" | "gemat" | "gescene") {
            continue;
        }
        let bytes = fs::read(&path)?;
        validate_semantics(&bytes)?;
        let parsed = format::parse(&bytes)?;
        assets.push(PackageAsset {
            asset_id: parsed.asset_id,
            content_hash: parsed.content_hash,
            magic: parsed.kind.magic(),
            path,
            normalized_name,
            bytes,
        });
    }
    assets.sort_by_key(|asset| asset.asset_id);
    for pair in assets.windows(2) {
        if pair[0].asset_id == pair[1].asset_id {
            return Err(AssetError::Invalid(
                "package contains duplicate asset IDs".into(),
            ));
        }
    }
    let mut normalized_names = HashSet::with_capacity(assets.len());
    for asset in &assets {
        if !normalized_names.insert(&asset.normalized_name) {
            return Err(AssetError::Invalid(
                "package contains duplicate normalized paths".into(),
            ));
        }
    }
    validate_package_references(&assets)?;
    if assets.is_empty() || assets.len() > format::MAX_CHUNKS - 2 {
        return Err(AssetError::Invalid(
            "package has no assets or too many assets".into(),
        ));
    }

    let mut package_entries = Vec::with_capacity(assets.len() * 40);
    let mut package_data = Vec::new();
    for asset in &assets {
        let offset = package_data.len() as u64;
        package_data.extend_from_slice(&asset.bytes);
        package_entries.extend_from_slice(&asset.asset_id.to_le_bytes());
        package_entries.extend_from_slice(&asset.content_hash.to_le_bytes());
        package_entries.extend_from_slice(&asset.magic);
        package_entries.extend_from_slice(&0_u32.to_le_bytes());
        package_entries.extend_from_slice(&offset.to_le_bytes());
        package_entries.extend_from_slice(&(asset.bytes.len() as u64).to_le_bytes());
        let name_hash = format::fnv1a64(asset.normalized_name.as_bytes());
        package_entries.extend_from_slice(&name_hash.to_le_bytes());
    }
    let bytes = format::build(
        AssetKind::Package,
        &[
            ChunkInput {
                kind: format::CHUNK_PACKAGE_ENTRY,
                data: package_entries,
            },
            ChunkInput {
                kind: format::CHUNK_PACKAGE_DATA,
                data: package_data,
            },
        ],
    )?;
    if let Some(parent) = output.parent().filter(|path| !path.as_os_str().is_empty()) {
        fs::create_dir_all(parent)?;
    }
    write_asset(output, &bytes)
}

fn run(args: &[String]) -> Result<()> {
    let command = args.get(1).map(String::as_str).unwrap_or_default();
    match command {
        "validate" => {
            let path = require_path(args.get(2), "asset path")?;
            validate_semantics(&fs::read(path)?).map(|_| println!("valid"))
        }
        "inspect" => {
            let path = require_path(args.get(2), "asset path")?;
            let bytes = fs::read(path)?;
            let asset = format::parse(&bytes)?;
            println!(
                "kind={} version={} asset_id={:016x} content_hash={:016x} chunks={}",
                asset.kind.extension(),
                asset.version,
                asset.asset_id,
                asset.content_hash,
                asset.chunks.len()
            );
            for chunk in asset.chunks {
                println!(
                    "  chunk={:08x} offset={} size={}",
                    chunk.kind,
                    chunk.offset,
                    chunk.data.len()
                );
            }
            Ok(())
        }
        "import-gltf" => {
            let input = require_path(args.get(2), "glTF path")?;
            let output_flag = args.iter().position(|value| value == "--output-dir");
            let output = require_path(
                output_flag.and_then(|index| args.get(index + 1)),
                "output directory",
            )?;
            let source = fs::read(&input)?;
            let cache_key = gameengine_asset_tool::cache::cache_key(&source, "import-gltf-json-v1");
            let (cached, _) = gameengine_asset_tool::cache::load_or_write(
                Path::new("build/asset-cache"),
                cache_key,
                "gltf-assets",
                || {
                    let assets = import_gltf(&input).map_err(|error| {
                        AssetError::Invalid(format!("{}: {error}", input.display()))
                    })?;
                    serialize_imported_assets(&assets)
                },
            )?;
            let assets = deserialize_imported_assets(&cached)?;
            write_imported_assets(&assets, &output)
        }
        "pack-mesh" | "pack-texture" | "pack-material" | "pack-scene" => {
            fixture_output(command, args.get(2))
        }
        "package" => {
            let input_flag = args.iter().position(|value| value == "--input-dir");
            let output_flag = args.iter().position(|value| value == "--output");
            let input = require_path(
                input_flag.and_then(|index| args.get(index + 1)),
                "input directory",
            )?;
            let output = require_path(
                output_flag.and_then(|index| args.get(index + 1)),
                "package path",
            )?;
            package_assets(&input, &output)
        }
        _ => {
            usage();
            Err(AssetError::Invalid("unknown or incomplete command".into()))
        }
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if let Err(error) = run(&args) {
        eprintln!("asset-tool: {error}");
        std::process::exit(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn imported_asset_cache_round_trips() {
        let asset = ImportedAsset {
            name: "fixture.gemesh".into(),
            kind: AssetKind::Mesh,
            bytes: pack_mesh(&fixture_mesh()).expect("mesh should pack"),
        };
        let serialized = serialize_imported_assets(&[asset]);
        let restored = deserialize_imported_assets(&serialized.expect("cache should serialize"))
            .expect("cache should deserialize");
        assert_eq!(restored.len(), 1);
        assert_eq!(restored[0].name, "fixture.gemesh");
        assert_eq!(restored[0].kind, AssetKind::Mesh);
    }

    #[test]
    fn package_reference_validation_accepts_and_rejects_expected_ids() {
        let mesh = pack_mesh(&fixture_mesh()).expect("mesh should pack");
        let mesh_id = format::parse(&mesh).expect("mesh should parse").asset_id;
        let material = pack_material(&fixture_material(0)).expect("material should pack");
        let material_id = format::parse(&material)
            .expect("material should parse")
            .asset_id;
        let scene = pack_scene(&fixture_scene(mesh_id, material_id)).expect("scene should pack");
        let scene_id = format::parse(&scene).expect("scene should parse").asset_id;
        let assets = vec![
            PackageAsset {
                asset_id: mesh_id,
                content_hash: 0,
                magic: format::MESH_MAGIC,
                path: PathBuf::from("mesh.gemesh"),
                normalized_name: "mesh.gemesh".into(),
                bytes: mesh,
            },
            PackageAsset {
                asset_id: material_id,
                content_hash: 0,
                magic: format::MATERIAL_MAGIC,
                path: PathBuf::from("material.gemat"),
                normalized_name: "material.gemat".into(),
                bytes: material,
            },
            PackageAsset {
                asset_id: scene_id,
                content_hash: 0,
                magic: format::SCENE_MAGIC,
                path: PathBuf::from("scene.gescene"),
                normalized_name: "scene.gescene".into(),
                bytes: scene,
            },
        ];
        validate_package_references(&assets).expect("references should be present");

        let invalid_scene = pack_scene(&fixture_scene(mesh_id, 99)).expect("scene should pack");
        let invalid_assets = vec![
            PackageAsset {
                asset_id: assets[0].asset_id,
                content_hash: 0,
                magic: assets[0].magic,
                path: assets[0].path.clone(),
                normalized_name: assets[0].normalized_name.clone(),
                bytes: assets[0].bytes.clone(),
            },
            PackageAsset {
                asset_id: assets[1].asset_id,
                content_hash: 0,
                magic: assets[1].magic,
                path: assets[1].path.clone(),
                normalized_name: assets[1].normalized_name.clone(),
                bytes: assets[1].bytes.clone(),
            },
            PackageAsset {
                asset_id: format::parse(&invalid_scene)
                    .expect("scene should parse")
                    .asset_id,
                content_hash: 0,
                magic: format::SCENE_MAGIC,
                path: PathBuf::from("invalid.gescene"),
                normalized_name: "invalid.gescene".into(),
                bytes: invalid_scene,
            },
        ];
        assert!(validate_package_references(&invalid_assets).is_err());
    }
}
