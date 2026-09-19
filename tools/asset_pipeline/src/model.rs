use std::path::Path;

use crate::format::{self, AssetKind, ChunkInput};
use crate::{AssetError, Result};

pub const VERTEX_FORMAT_POSITION_NORMAL_UV: u32 = 1;
pub const INDEX_FORMAT_U16: u32 = 16;
pub const INDEX_FORMAT_U32: u32 = 32;
pub const TEXTURE_FORMAT_RGBA8_UNORM: u32 = 1;

#[derive(Clone, Debug)]
pub struct SubmeshInput {
    pub first_index: u32,
    pub index_count: u32,
    pub material_slot: u32,
    pub vertex_offset: u32,
}

#[derive(Clone, Debug)]
pub struct MeshInput {
    pub vertices: Vec<u8>,
    pub vertex_count: u32,
    pub vertex_stride: u32,
    pub indices: Vec<u8>,
    pub index_count: u32,
    pub index_format: u32,
    pub submeshes: Vec<SubmeshInput>,
}

#[derive(Clone, Debug)]
pub struct TextureInput {
    pub width: u32,
    pub height: u32,
    pub pixels: Vec<u8>,
}

#[derive(Clone, Debug)]
pub struct MaterialInput {
    pub base_color_factor: [f32; 4],
    pub metallic: f32,
    pub roughness: f32,
    pub albedo_texture: u64,
    pub normal_texture: u64,
    pub orm_texture: u64,
}

#[derive(Clone, Debug)]
pub struct SceneInstance {
    pub transform: [f32; 16],
    pub mesh_id: u64,
    pub material_id: u64,
}

#[derive(Clone, Debug)]
pub struct SceneInput {
    pub instances: Vec<SceneInstance>,
}

fn write_u32(output: &mut Vec<u8>, value: u32) {
    output.extend_from_slice(&value.to_le_bytes());
}

fn write_u64(output: &mut Vec<u8>, value: u64) {
    output.extend_from_slice(&value.to_le_bytes());
}

fn write_f32(output: &mut Vec<u8>, value: f32) {
    output.extend_from_slice(&value.to_le_bytes());
}

fn write_f32_array<const N: usize>(output: &mut Vec<u8>, values: &[f32; N]) {
    for value in values {
        write_f32(output, *value);
    }
}

pub fn pack_mesh(input: &MeshInput) -> Result<Vec<u8>> {
    if input.vertex_stride != 32 || input.vertex_count == 0 {
        return Err(AssetError::Invalid(
            "mesh requires position3_normal3_uv2 vertices".into(),
        ));
    }
    let expected_vertices = usize::try_from(input.vertex_count)
        .ok()
        .and_then(|count| count.checked_mul(input.vertex_stride as usize))
        .ok_or_else(|| AssetError::Invalid("mesh vertex size overflow".into()))?;
    if expected_vertices != input.vertices.len() {
        return Err(AssetError::Invalid(
            "mesh vertex payload size mismatch".into(),
        ));
    }
    let index_width = match input.index_format {
        INDEX_FORMAT_U16 => 2,
        INDEX_FORMAT_U32 => 4,
        _ => return Err(AssetError::Invalid("unsupported mesh index format".into())),
    };
    let expected_indices = usize::try_from(input.index_count)
        .ok()
        .and_then(|count| count.checked_mul(index_width))
        .ok_or_else(|| AssetError::Invalid("mesh index size overflow".into()))?;
    if expected_indices != input.indices.len() {
        return Err(AssetError::Invalid(
            "mesh index payload size mismatch".into(),
        ));
    }
    for index in 0..input.index_count as usize {
        let value = if input.index_format == INDEX_FORMAT_U16 {
            u16::from_le_bytes(
                input.indices[index * 2..index * 2 + 2]
                    .try_into()
                    .expect("checked length"),
            ) as u32
        } else {
            u32::from_le_bytes(
                input.indices[index * 4..index * 4 + 4]
                    .try_into()
                    .expect("checked length"),
            )
        };
        if value >= input.vertex_count {
            return Err(AssetError::Invalid(format!(
                "mesh index {value} is out of range"
            )));
        }
    }
    for submesh in &input.submeshes {
        let end = submesh
            .first_index
            .checked_add(submesh.index_count)
            .ok_or_else(|| AssetError::Invalid("submesh index range overflow".into()))?;
        if end > input.index_count {
            return Err(AssetError::Invalid(
                "submesh index range is out of bounds".into(),
            ));
        }
        if submesh.vertex_offset >= input.vertex_count {
            return Err(AssetError::Invalid(
                "submesh vertex offset is out of bounds".into(),
            ));
        }
    }
    let mut metadata = Vec::with_capacity(24);
    write_u32(&mut metadata, VERTEX_FORMAT_POSITION_NORMAL_UV);
    write_u32(&mut metadata, input.vertex_count);
    write_u32(&mut metadata, input.vertex_stride);
    write_u32(&mut metadata, input.index_format);
    write_u32(&mut metadata, input.index_count);
    write_u32(&mut metadata, input.submeshes.len() as u32);
    let mut submeshes = Vec::with_capacity(input.submeshes.len() * 16);
    for submesh in &input.submeshes {
        write_u32(&mut submeshes, submesh.first_index);
        write_u32(&mut submeshes, submesh.index_count);
        write_u32(&mut submeshes, submesh.material_slot);
        write_u32(&mut submeshes, submesh.vertex_offset);
    }
    format::build(
        AssetKind::Mesh,
        &[
            ChunkInput {
                kind: format::CHUNK_MESH_HEADER,
                data: metadata,
            },
            ChunkInput {
                kind: format::CHUNK_VERTICES,
                data: input.vertices.clone(),
            },
            ChunkInput {
                kind: format::CHUNK_INDICES,
                data: input.indices.clone(),
            },
            ChunkInput {
                kind: format::CHUNK_SUBMESHES,
                data: submeshes,
            },
        ],
    )
}

pub fn pack_texture(input: &TextureInput) -> Result<Vec<u8>> {
    if input.width == 0 || input.height == 0 || input.width > 16384 || input.height > 16384 {
        return Err(AssetError::Invalid("texture dimensions are invalid".into()));
    }
    let pixel_count = (input.width as usize)
        .checked_mul(input.height as usize)
        .and_then(|count| count.checked_mul(4))
        .ok_or_else(|| AssetError::Invalid("texture size overflow".into()))?;
    if input.pixels.len() != pixel_count {
        return Err(AssetError::Invalid(
            "texture pixel payload size mismatch".into(),
        ));
    }
    let mut metadata = Vec::with_capacity(20);
    write_u32(&mut metadata, input.width);
    write_u32(&mut metadata, input.height);
    write_u32(&mut metadata, 1);
    write_u32(&mut metadata, TEXTURE_FORMAT_RGBA8_UNORM);
    write_u32(&mut metadata, input.pixels.len() as u32);
    format::build(
        AssetKind::Texture,
        &[
            ChunkInput {
                kind: format::CHUNK_TEXTURE_HEADER,
                data: metadata,
            },
            ChunkInput {
                kind: format::CHUNK_TEXTURE_DATA,
                data: input.pixels.clone(),
            },
        ],
    )
}

pub fn pack_material(input: &MaterialInput) -> Result<Vec<u8>> {
    if !input
        .base_color_factor
        .iter()
        .all(|value| value.is_finite())
        || !input.metallic.is_finite()
        || !input.roughness.is_finite()
        || !(0.0..=1.0).contains(&input.metallic)
        || !(0.0..=1.0).contains(&input.roughness)
    {
        return Err(AssetError::Invalid("material values are invalid".into()));
    }
    let mut metadata = Vec::with_capacity(64);
    write_f32_array(&mut metadata, &input.base_color_factor);
    write_f32(&mut metadata, input.metallic);
    write_f32(&mut metadata, input.roughness);
    write_u64(&mut metadata, input.albedo_texture);
    write_u64(&mut metadata, input.normal_texture);
    write_u64(&mut metadata, input.orm_texture);
    write_u64(&mut metadata, 0);
    metadata.resize(64, 0);
    format::build(
        AssetKind::Material,
        &[ChunkInput {
            kind: format::CHUNK_MATERIAL_HEADER,
            data: metadata,
        }],
    )
}

pub fn pack_scene(input: &SceneInput) -> Result<Vec<u8>> {
    if input.instances.len() > 1_000_000 {
        return Err(AssetError::Invalid("scene has too many instances".into()));
    }
    let mut header = Vec::with_capacity(8);
    write_u32(&mut header, input.instances.len() as u32);
    write_u32(&mut header, 0);
    let mut instances = Vec::with_capacity(input.instances.len() * 80);
    for instance in &input.instances {
        if !instance.transform.iter().all(|value| value.is_finite()) {
            return Err(AssetError::Invalid(
                "scene transform contains a non-finite value".into(),
            ));
        }
        write_f32_array(&mut instances, &instance.transform);
        write_u64(&mut instances, instance.mesh_id);
        write_u64(&mut instances, instance.material_id);
    }
    format::build(
        AssetKind::Scene,
        &[
            ChunkInput {
                kind: format::CHUNK_SCENE_HEADER,
                data: header,
            },
            ChunkInput {
                kind: format::CHUNK_INSTANCES,
                data: instances,
            },
        ],
    )
}

pub fn fixture_mesh() -> MeshInput {
    let vertices = vec![
        -1.0_f32, -1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, -1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0,
        1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
    ]
    .into_iter()
    .flat_map(f32::to_le_bytes)
    .collect();
    MeshInput {
        vertices,
        vertex_count: 4,
        vertex_stride: 32,
        indices: [0_u16, 1, 2, 2, 3, 0]
            .into_iter()
            .flat_map(u16::to_le_bytes)
            .collect(),
        index_count: 6,
        index_format: INDEX_FORMAT_U16,
        submeshes: vec![SubmeshInput {
            first_index: 0,
            index_count: 6,
            material_slot: 0,
            vertex_offset: 0,
        }],
    }
}

pub fn fixture_texture() -> TextureInput {
    TextureInput {
        width: 2,
        height: 2,
        pixels: vec![
            255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255,
        ],
    }
}

pub fn fixture_material(texture_id: u64) -> MaterialInput {
    MaterialInput {
        base_color_factor: [1.0, 1.0, 1.0, 1.0],
        metallic: 0.1,
        roughness: 0.5,
        albedo_texture: texture_id,
        normal_texture: 0,
        orm_texture: 0,
    }
}

pub fn fixture_scene(mesh_id: u64, material_id: u64) -> SceneInput {
    SceneInput {
        instances: vec![SceneInstance {
            transform: [
                1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
            ],
            mesh_id,
            material_id,
        }],
    }
}

pub fn write_asset(path: &Path, bytes: &[u8]) -> Result<()> {
    let temporary = path.with_extension(format!(
        "{}.tmp",
        path.extension()
            .and_then(|value| value.to_str())
            .unwrap_or("asset")
    ));
    std::fs::write(&temporary, bytes)?;
    std::fs::rename(temporary, path)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::format;

    #[test]
    fn fixture_formats_round_trip() {
        let mesh = pack_mesh(&fixture_mesh()).expect("mesh should pack");
        let texture = pack_texture(&fixture_texture()).expect("texture should pack");
        let material = pack_material(&fixture_material(7)).expect("material should pack");
        let scene = pack_scene(&fixture_scene(11, 13)).expect("scene should pack");
        assert_eq!(
            format::parse(&mesh).expect("mesh should parse").kind,
            AssetKind::Mesh
        );
        assert_eq!(
            format::parse(&texture).expect("texture should parse").kind,
            AssetKind::Texture
        );
        assert_eq!(
            format::parse(&material)
                .expect("material should parse")
                .kind,
            AssetKind::Material
        );
        assert_eq!(
            format::parse(&scene).expect("scene should parse").kind,
            AssetKind::Scene
        );
        assert_eq!(
            mesh,
            pack_mesh(&fixture_mesh()).expect("mesh should be deterministic")
        );
    }

    #[test]
    fn rejects_invalid_texture_and_indices() {
        assert!(pack_texture(&TextureInput {
            width: 0,
            height: 1,
            pixels: vec![]
        })
        .is_err());
        let mut mesh = fixture_mesh();
        mesh.indices[0] = 0xff;
        mesh.indices[1] = 0xff;
        assert!(pack_mesh(&mesh).is_err());
    }
}
