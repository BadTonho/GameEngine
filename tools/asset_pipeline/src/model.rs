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

#[derive(Clone, Debug)]
pub struct SceneEntity {
    pub value: u64,
    pub parent: u64,
}

#[derive(Clone, Debug)]
pub struct SceneTransform {
    pub entity: u64,
    pub position: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
}

#[derive(Clone, Debug)]
pub struct SceneMeshRenderer {
    pub entity: u64,
    pub mesh_id: u64,
    pub material_id: u64,
}

#[derive(Clone, Debug)]
pub struct SceneCamera {
    pub entity: u64,
    pub fov_y: f32,
    pub near_plane: f32,
    pub far_plane: f32,
    pub active: bool,
}

#[derive(Clone, Debug)]
pub struct SceneLight {
    pub entity: u64,
    pub direction: [f32; 3],
    pub color: [f32; 3],
    pub intensity: f32,
}

#[derive(Clone, Debug, Default)]
pub struct SceneGraphInput {
    pub entities: Vec<SceneEntity>,
    pub transforms: Vec<SceneTransform>,
    pub mesh_renderers: Vec<SceneMeshRenderer>,
    pub cameras: Vec<SceneCamera>,
    pub lights: Vec<SceneLight>,
    pub active_camera: u64,
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

pub fn pack_scene_graph(input: &SceneGraphInput) -> Result<Vec<u8>> {
    if input.entities.is_empty()
        || input.entities.len() > 1_000_000
        || input.transforms.len() != input.entities.len()
    {
        return Err(AssetError::Invalid(
            "scene graph requires one transform for every entity".into(),
        ));
    }
    let mut entities = input.entities.clone();
    entities.sort_by_key(|entity| entity.value);
    if entities.iter().any(|entity| entity.value == 0)
        || entities
            .windows(2)
            .any(|pair| pair[0].value == pair[1].value)
    {
        return Err(AssetError::Invalid(
            "scene graph has duplicate or invalid entities".into(),
        ));
    }
    let entity_ids: Vec<u64> = entities.iter().map(|entity| entity.value).collect();
    if input.active_camera != 0 && !entity_ids.contains(&input.active_camera) {
        return Err(AssetError::Invalid("scene active camera is missing".into()));
    }
    if input
        .entities
        .iter()
        .any(|entity| entity.parent != 0 && !entity_ids.contains(&entity.parent))
    {
        return Err(AssetError::Invalid(
            "scene parent references a missing entity".into(),
        ));
    }
    for entity in &entities {
        let mut current = entity.value;
        let mut reached_root = false;
        for _ in 0..entities.len() {
            let parent = entities
                .iter()
                .find(|candidate| candidate.value == current)
                .map(|candidate| candidate.parent)
                .unwrap_or(0);
            if parent == 0 {
                reached_root = true;
                break;
            }
            current = parent;
        }
        if !reached_root {
            return Err(AssetError::Invalid(
                "scene hierarchy contains a cycle".into(),
            ));
        }
    }
    let mut transforms = input.transforms.clone();
    transforms.sort_by_key(|transform| transform.entity);
    if transforms.iter().any(|transform| {
        !entity_ids.contains(&transform.entity)
            || !transform
                .position
                .iter()
                .chain(transform.rotation.iter())
                .chain(transform.scale.iter())
                .all(|value| value.is_finite())
            || transform.rotation.iter().all(|value| *value == 0.0)
    }) || transforms
        .windows(2)
        .any(|pair| pair[0].entity == pair[1].entity)
    {
        return Err(AssetError::Invalid(
            "scene transform data is invalid".into(),
        ));
    }
    let mut mesh_renderers = input.mesh_renderers.clone();
    mesh_renderers.sort_by_key(|renderer| renderer.entity);
    let mut cameras = input.cameras.clone();
    cameras.sort_by_key(|camera| camera.entity);
    let mut lights = input.lights.clone();
    lights.sort_by_key(|light| light.entity);
    if mesh_renderers
        .iter()
        .any(|renderer| !entity_ids.contains(&renderer.entity))
        || cameras.iter().any(|camera| {
            !entity_ids.contains(&camera.entity)
                || !camera.fov_y.is_finite()
                || !camera.near_plane.is_finite()
                || !camera.far_plane.is_finite()
                || camera.fov_y <= 0.0
                || camera.near_plane <= 0.0
                || camera.far_plane <= camera.near_plane
        })
        || lights.iter().any(|light| {
            !entity_ids.contains(&light.entity)
                || !light.direction.iter().all(|value| value.is_finite())
                || !light.color.iter().all(|value| value.is_finite())
                || !light.intensity.is_finite()
                || light.intensity < 0.0
        })
    {
        return Err(AssetError::Invalid(
            "scene component references or values are invalid".into(),
        ));
    }
    if cameras.iter().filter(|camera| camera.active).count() > 1
        || (input.active_camera != 0
            && !cameras
                .iter()
                .any(|camera| camera.entity == input.active_camera && camera.active))
    {
        return Err(AssetError::Invalid(
            "scene active camera metadata is inconsistent".into(),
        ));
    }

    let mut header = Vec::with_capacity(16);
    write_u32(&mut header, entities.len() as u32);
    write_u32(&mut header, 0);
    write_u64(&mut header, input.active_camera);
    let mut entity_data = Vec::with_capacity(entities.len() * 16);
    for entity in &entities {
        write_u64(&mut entity_data, entity.value);
        write_u64(&mut entity_data, entity.parent);
    }
    let mut transform_data = Vec::with_capacity(transforms.len() * 48);
    for transform in &transforms {
        write_u64(&mut transform_data, transform.entity);
        write_f32_array(&mut transform_data, &transform.position);
        write_f32_array(&mut transform_data, &transform.rotation);
        write_f32_array(&mut transform_data, &transform.scale);
    }
    let mut mesh_data = Vec::with_capacity(mesh_renderers.len() * 24);
    for renderer in &mesh_renderers {
        write_u64(&mut mesh_data, renderer.entity);
        write_u64(&mut mesh_data, renderer.mesh_id);
        write_u64(&mut mesh_data, renderer.material_id);
    }
    let mut camera_data = Vec::with_capacity(cameras.len() * 24);
    for camera in &cameras {
        write_u64(&mut camera_data, camera.entity);
        write_f32(&mut camera_data, camera.fov_y);
        write_f32(&mut camera_data, camera.near_plane);
        write_f32(&mut camera_data, camera.far_plane);
        write_u32(&mut camera_data, u32::from(camera.active));
    }
    let mut light_data = Vec::with_capacity(lights.len() * 40);
    for light in &lights {
        write_u64(&mut light_data, light.entity);
        write_f32_array(&mut light_data, &light.direction);
        write_f32_array(&mut light_data, &light.color);
        write_f32(&mut light_data, light.intensity);
        write_u32(&mut light_data, 0);
    }
    let mut chunks = vec![
        ChunkInput {
            kind: format::CHUNK_SCENE_HEADER,
            data: header,
        },
        ChunkInput {
            kind: format::CHUNK_SCENE_ENTITIES,
            data: entity_data,
        },
        ChunkInput {
            kind: format::CHUNK_SCENE_TRANSFORMS,
            data: transform_data,
        },
        ChunkInput {
            kind: format::CHUNK_SCENE_MESH_RENDERERS,
            data: mesh_data,
        },
    ];
    if !camera_data.is_empty() {
        chunks.push(ChunkInput {
            kind: format::CHUNK_SCENE_CAMERAS,
            data: camera_data,
        });
    }
    if !light_data.is_empty() {
        chunks.push(ChunkInput {
            kind: format::CHUNK_SCENE_LIGHTS,
            data: light_data,
        });
    }
    format::build(AssetKind::Scene, &chunks)
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

pub fn fixture_scene_graph(mesh_id: u64, material_id: u64) -> SceneGraphInput {
    SceneGraphInput {
        entities: vec![SceneEntity {
            value: 1_u64 << 32,
            parent: 0,
        }],
        transforms: vec![SceneTransform {
            entity: 1_u64 << 32,
            position: [0.0, 0.0, 0.0],
            rotation: [0.0, 0.0, 0.0, 1.0],
            scale: [1.0, 1.0, 1.0],
        }],
        mesh_renderers: vec![SceneMeshRenderer {
            entity: 1_u64 << 32,
            mesh_id,
            material_id,
        }],
        cameras: vec![],
        lights: vec![],
        active_camera: 0,
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
        let graph = pack_scene_graph(&fixture_scene_graph(11, 13)).expect("graph should pack");
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
            format::parse(&graph).expect("graph should parse").kind,
            AssetKind::Scene
        );
        assert_eq!(
            mesh,
            pack_mesh(&fixture_mesh()).expect("mesh should be deterministic")
        );
        assert_eq!(
            graph,
            pack_scene_graph(&fixture_scene_graph(11, 13)).expect("graph should be deterministic")
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
