use std::collections::HashMap;
use std::path::{Component, Path};

use serde::Deserialize;
use serde_json::Value;

use crate::format::AssetKind;
use crate::model::{
    pack_material, pack_mesh, write_asset, MaterialInput, MeshInput, SubmeshInput,
    INDEX_FORMAT_U16, INDEX_FORMAT_U32,
};
use crate::{AssetError, Result};

#[derive(Debug)]
pub struct ImportedAsset {
    pub name: String,
    pub kind: AssetKind,
    pub bytes: Vec<u8>,
}

#[derive(Debug, Deserialize)]
struct GltfDocument {
    #[serde(default)]
    buffers: Vec<GltfBuffer>,
    #[serde(rename = "bufferViews", default)]
    buffer_views: Vec<GltfBufferView>,
    #[serde(default)]
    accessors: Vec<GltfAccessor>,
    #[serde(default)]
    meshes: Vec<GltfMesh>,
    #[serde(default)]
    materials: Vec<GltfMaterial>,
    #[serde(default)]
    images: Vec<Value>,
    #[serde(default)]
    animations: Vec<Value>,
    #[serde(default)]
    skins: Vec<Value>,
}

#[derive(Debug, Deserialize)]
struct GltfBuffer {
    uri: Option<String>,
    #[serde(rename = "byteLength")]
    byte_length: usize,
}

#[derive(Debug, Deserialize)]
struct GltfBufferView {
    buffer: usize,
    #[serde(rename = "byteOffset", default)]
    byte_offset: usize,
    #[serde(rename = "byteLength")]
    byte_length: usize,
    #[serde(rename = "byteStride")]
    byte_stride: Option<usize>,
}

#[derive(Debug, Deserialize)]
struct GltfAccessor {
    #[serde(rename = "bufferView")]
    buffer_view: Option<usize>,
    #[serde(rename = "byteOffset", default)]
    byte_offset: usize,
    #[serde(rename = "componentType")]
    component_type: u32,
    count: usize,
    #[serde(rename = "type")]
    type_name: String,
    #[serde(default)]
    normalized: bool,
    #[serde(default)]
    sparse: Option<Value>,
}

#[derive(Debug, Deserialize)]
struct GltfMesh {
    #[serde(default)]
    primitives: Vec<GltfPrimitive>,
}

#[derive(Debug, Deserialize)]
struct GltfPrimitive {
    attributes: HashMap<String, usize>,
    indices: Option<usize>,
    material: Option<usize>,
    mode: Option<u32>,
    #[serde(default)]
    targets: Vec<Value>,
    extensions: Option<HashMap<String, Value>>,
}

#[derive(Debug, Deserialize)]
struct GltfMaterial {
    #[serde(rename = "pbrMetallicRoughness", default)]
    pbr: Option<GltfPbr>,
}

#[derive(Debug, Deserialize)]
struct GltfPbr {
    #[serde(rename = "baseColorFactor", default = "default_color")]
    base_color_factor: [f32; 4],
    #[serde(rename = "metallicFactor", default = "default_metallic")]
    metallic: f32,
    #[serde(rename = "roughnessFactor", default = "default_roughness")]
    roughness: f32,
    #[serde(rename = "baseColorTexture")]
    base_color_texture: Option<Value>,
}

fn default_color() -> [f32; 4] {
    [1.0, 1.0, 1.0, 1.0]
}

fn default_metallic() -> f32 {
    1.0
}

fn default_roughness() -> f32 {
    1.0
}

fn decode_base64(value: &str) -> Result<Vec<u8>> {
    let mut output = Vec::new();
    let mut accumulator = 0_u32;
    let mut bits = 0_u32;
    for character in value.bytes() {
        if character == b'=' {
            break;
        }
        let digit = match character {
            b'A'..=b'Z' => character - b'A',
            b'a'..=b'z' => character - b'a' + 26,
            b'0'..=b'9' => character - b'0' + 52,
            b'+' => 62,
            b'/' => 63,
            b'\r' | b'\n' | b' ' | b'\t' => continue,
            _ => return Err(AssetError::Invalid("invalid base64 buffer URI".into())),
        };
        accumulator = (accumulator << 6) | u32::from(digit);
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            output.push((accumulator >> bits) as u8);
        }
    }
    Ok(output)
}

fn load_buffer(buffer: &GltfBuffer, base_dir: &Path) -> Result<Vec<u8>> {
    let bytes = match buffer.uri.as_deref() {
        None => {
            return Err(AssetError::Invalid(
                "binary GLB buffers are unsupported; use JSON glTF".into(),
            ))
        }
        Some(uri) if uri.starts_with("data:application/octet-stream;base64,") => {
            decode_base64(&uri[37..])?
        }
        Some(uri) if uri.starts_with("data:") => {
            return Err(AssetError::Invalid(
                "only base64 octet-stream data URIs are supported".into(),
            ))
        }
        Some(uri) => {
            let path = Path::new(uri);
            if path.is_absolute()
                || path
                    .components()
                    .any(|component| component == Component::ParentDir)
            {
                return Err(AssetError::Invalid(
                    "buffer path escapes the glTF directory".into(),
                ));
            }
            std::fs::read(base_dir.join(path))?
        }
    };
    if bytes.len() < buffer.byte_length {
        return Err(AssetError::Invalid(
            "buffer is shorter than byteLength".into(),
        ));
    }
    Ok(bytes)
}

fn accessor_range<'a>(
    accessor: &GltfAccessor,
    views: &[GltfBufferView],
    buffers: &'a [Vec<u8>],
) -> Result<(&'a [u8], usize)> {
    if accessor.normalized {
        return Err(AssetError::Invalid(
            "normalized accessors are unsupported".into(),
        ));
    }
    if accessor.sparse.is_some() {
        return Err(AssetError::Invalid(
            "sparse accessors are unsupported".into(),
        ));
    }
    let view_index = accessor
        .buffer_view
        .ok_or_else(|| AssetError::Invalid("sparse accessors are unsupported".into()))?;
    let view = views
        .get(view_index)
        .ok_or_else(|| AssetError::Invalid("accessor bufferView is out of range".into()))?;
    let buffer = buffers
        .get(view.buffer)
        .ok_or_else(|| AssetError::Invalid("bufferView buffer is out of range".into()))?;
    let view_end = view
        .byte_offset
        .checked_add(view.byte_length)
        .ok_or_else(|| AssetError::Invalid("bufferView range overflow".into()))?;
    if view_end > buffer.len() {
        return Err(AssetError::Invalid(
            "bufferView is outside its buffer".into(),
        ));
    }
    let component_size = match accessor.component_type {
        5123 | 5125 => 2,
        5126 => 4,
        _ => {
            return Err(AssetError::Invalid(
                "unsupported accessor component type".into(),
            ))
        }
    };
    let component_count = match accessor.type_name.as_str() {
        "SCALAR" => 1,
        "VEC2" => 2,
        "VEC3" => 3,
        _ => return Err(AssetError::Invalid("unsupported accessor type".into())),
    };
    let element_size = component_size * component_count;
    let stride = view.byte_stride.unwrap_or(element_size);
    if stride < element_size {
        return Err(AssetError::Invalid(
            "accessor byteStride is smaller than its element".into(),
        ));
    }
    let required = accessor
        .count
        .saturating_sub(1)
        .checked_mul(stride)
        .and_then(|value| value.checked_add(element_size))
        .and_then(|value| value.checked_add(accessor.byte_offset))
        .ok_or_else(|| AssetError::Invalid("accessor range overflow".into()))?;
    if required > view.byte_length {
        return Err(AssetError::Invalid(
            "accessor is outside its bufferView".into(),
        ));
    }
    let start = view.byte_offset + accessor.byte_offset;
    Ok((&buffer[start..view_end], stride))
}

fn read_f32(bytes: &[u8], offset: usize) -> Result<f32> {
    let end = offset
        .checked_add(4)
        .ok_or_else(|| AssetError::Invalid("float range overflow".into()))?;
    let value = bytes
        .get(offset..end)
        .ok_or_else(|| AssetError::Invalid("float is truncated".into()))?;
    Ok(f32::from_le_bytes(
        value.try_into().expect("checked length"),
    ))
}

fn read_vec3(
    accessor: &GltfAccessor,
    views: &[GltfBufferView],
    buffers: &[Vec<u8>],
) -> Result<Vec<[f32; 3]>> {
    if accessor.component_type != 5126 || accessor.type_name != "VEC3" {
        return Err(AssetError::Invalid(
            "mesh attribute must be float VEC3".into(),
        ));
    }
    let (bytes, stride) = accessor_range(accessor, views, buffers)?;
    (0..accessor.count)
        .map(|index| {
            let offset = index * stride;
            Ok([
                read_f32(bytes, offset)?,
                read_f32(bytes, offset + 4)?,
                read_f32(bytes, offset + 8)?,
            ])
        })
        .collect()
}

fn read_vec2(
    accessor: &GltfAccessor,
    views: &[GltfBufferView],
    buffers: &[Vec<u8>],
) -> Result<Vec<[f32; 2]>> {
    if accessor.component_type != 5126 || accessor.type_name != "VEC2" {
        return Err(AssetError::Invalid(
            "mesh attribute must be float VEC2".into(),
        ));
    }
    let (bytes, stride) = accessor_range(accessor, views, buffers)?;
    (0..accessor.count)
        .map(|index| {
            let offset = index * stride;
            Ok([read_f32(bytes, offset)?, read_f32(bytes, offset + 4)?])
        })
        .collect()
}

fn read_indices(
    accessor: &GltfAccessor,
    views: &[GltfBufferView],
    buffers: &[Vec<u8>],
) -> Result<Vec<u32>> {
    if accessor.type_name != "SCALAR"
        || (accessor.component_type != 5123 && accessor.component_type != 5125)
    {
        return Err(AssetError::Invalid(
            "indices must be uint16 or uint32 scalar".into(),
        ));
    }
    let (bytes, stride) = accessor_range(accessor, views, buffers)?;
    let width = if accessor.component_type == 5123 {
        2
    } else {
        4
    };
    (0..accessor.count)
        .map(|index| {
            let offset = index * stride;
            if width == 2 {
                Ok(u16::from_le_bytes(
                    bytes[offset..offset + 2]
                        .try_into()
                        .expect("checked length"),
                ) as u32)
            } else {
                Ok(u32::from_le_bytes(
                    bytes[offset..offset + 4]
                        .try_into()
                        .expect("checked length"),
                ))
            }
        })
        .collect()
}

fn append_f32(output: &mut Vec<u8>, value: f32) {
    output.extend_from_slice(&value.to_le_bytes());
}

pub fn import_gltf(path: &Path) -> Result<Vec<ImportedAsset>> {
    let source = std::fs::read_to_string(path)?;
    let document: GltfDocument = serde_json::from_str(&source)?;
    if !document.images.is_empty() {
        return Err(AssetError::Invalid(
            "image decoding is not part of the code-only importer".into(),
        ));
    }
    if !document.animations.is_empty() || !document.skins.is_empty() {
        return Err(AssetError::Invalid(
            "animations and skins are unsupported".into(),
        ));
    }
    let base_dir = path.parent().unwrap_or_else(|| Path::new("."));
    let buffers: Vec<Vec<u8>> = document
        .buffers
        .iter()
        .map(|buffer| load_buffer(buffer, base_dir))
        .collect::<Result<_>>()?;
    let mut assets = Vec::new();
    for (mesh_index, mesh) in document.meshes.iter().enumerate() {
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        let mut submeshes = Vec::new();
        for primitive in &mesh.primitives {
            if primitive.mode.unwrap_or(4) != 4 {
                return Err(AssetError::Invalid(format!(
                    "mesh {mesh_index} contains a non-triangle primitive"
                )));
            }
            if !primitive.targets.is_empty() {
                return Err(AssetError::Invalid(format!(
                    "mesh {mesh_index} contains morph targets"
                )));
            }
            if primitive
                .extensions
                .as_ref()
                .is_some_and(|extensions| extensions.contains_key("KHR_draco_mesh_compression"))
            {
                return Err(AssetError::Invalid(format!(
                    "mesh {mesh_index} uses Draco compression"
                )));
            }
            let position_accessor = primitive
                .attributes
                .get("POSITION")
                .ok_or_else(|| AssetError::Invalid(format!("mesh {mesh_index} has no POSITION")))?;
            let normal_accessor = primitive
                .attributes
                .get("NORMAL")
                .ok_or_else(|| AssetError::Invalid(format!("mesh {mesh_index} has no NORMAL")))?;
            let uv_accessor = primitive.attributes.get("TEXCOORD_0").ok_or_else(|| {
                AssetError::Invalid(format!("mesh {mesh_index} has no TEXCOORD_0"))
            })?;
            let positions = read_vec3(
                document.accessors.get(*position_accessor).ok_or_else(|| {
                    AssetError::Invalid("POSITION accessor is out of range".into())
                })?,
                &document.buffer_views,
                &buffers,
            )?;
            let normals = read_vec3(
                document
                    .accessors
                    .get(*normal_accessor)
                    .ok_or_else(|| AssetError::Invalid("NORMAL accessor is out of range".into()))?,
                &document.buffer_views,
                &buffers,
            )?;
            let uvs = read_vec2(
                document.accessors.get(*uv_accessor).ok_or_else(|| {
                    AssetError::Invalid("TEXCOORD_0 accessor is out of range".into())
                })?,
                &document.buffer_views,
                &buffers,
            )?;
            if positions.len() != normals.len() || positions.len() != uvs.len() {
                return Err(AssetError::Invalid(format!(
                    "mesh {mesh_index} attribute counts differ"
                )));
            }
            for ((position, normal), uv) in positions.iter().zip(normals.iter()).zip(uvs.iter()) {
                for value in position.iter().chain(normal.iter()).chain(uv.iter()) {
                    append_f32(&mut vertices, *value);
                }
            }
            let vertex_offset = (vertices.len() / 32 - positions.len()) as u32;
            let first_index = indices.len() as u32;
            let primitive_indices = match primitive.indices {
                Some(index) => read_indices(
                    document.accessors.get(index).ok_or_else(|| {
                        AssetError::Invalid("index accessor is out of range".into())
                    })?,
                    &document.buffer_views,
                    &buffers,
                )?,
                None => (0..positions.len() as u32).collect(),
            };
            let maximum_index = primitive_indices.iter().copied().max().unwrap_or(0);
            if maximum_index >= positions.len() as u32 {
                return Err(AssetError::Invalid(format!(
                    "mesh {mesh_index} index is out of range"
                )));
            }
            indices.extend(primitive_indices);
            let material_slot = match primitive.material {
                Some(material_index) if material_index >= document.materials.len() => {
                    return Err(AssetError::Invalid(format!(
                        "mesh {mesh_index} references a material outside the document"
                    )))
                }
                Some(material_index) => material_index as u32,
                None => 0,
            };
            submeshes.push(SubmeshInput {
                first_index,
                index_count: indices.len() as u32 - first_index,
                material_slot,
                vertex_offset,
            });
        }
        let vertex_count = (vertices.len() / 32) as u32;
        let index_count = indices.len() as u32;
        let index_format = if vertex_count <= u32::from(u16::MAX) {
            INDEX_FORMAT_U16
        } else {
            INDEX_FORMAT_U32
        };
        let index_bytes = if index_format == INDEX_FORMAT_U16 {
            indices
                .iter()
                .map(|index| {
                    u16::try_from(*index)
                        .map(|value| value.to_le_bytes().to_vec())
                        .map_err(|_| AssetError::Invalid("mesh requires uint32 indices".into()))
                })
                .collect::<Result<Vec<_>>>()?
                .into_iter()
                .flatten()
                .collect()
        } else {
            indices
                .iter()
                .flat_map(|index| index.to_le_bytes())
                .collect()
        };
        let mesh = MeshInput {
            vertices,
            vertex_count,
            vertex_stride: 32,
            indices: index_bytes,
            index_count,
            index_format,
            submeshes,
        };
        assets.push(ImportedAsset {
            name: format!("mesh_{mesh_index}.gemesh"),
            kind: AssetKind::Mesh,
            bytes: pack_mesh(&mesh)?,
        });
    }
    for (material_index, material) in document.materials.iter().enumerate() {
        let pbr = material.pbr.as_ref();
        if pbr
            .and_then(|value| value.base_color_texture.as_ref())
            .is_some()
        {
            return Err(AssetError::Invalid(
                "glTF texture references require a texture compiler input".into(),
            ));
        }
        assets.push(ImportedAsset {
            name: format!("material_{material_index}.gemat"),
            kind: AssetKind::Material,
            bytes: pack_material(&MaterialInput {
                base_color_factor: pbr.map_or([1.0; 4], |value| value.base_color_factor),
                metallic: pbr.map_or(1.0, |value| value.metallic),
                roughness: pbr.map_or(1.0, |value| value.roughness),
                albedo_texture: 0,
                normal_texture: 0,
                orm_texture: 0,
            })?,
        });
    }
    Ok(assets)
}

pub fn write_imported_assets(assets: &[ImportedAsset], output_dir: &Path) -> Result<()> {
    std::fs::create_dir_all(output_dir)?;
    for asset in assets {
        write_asset(&output_dir.join(&asset.name), &asset.bytes)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::format;

    fn encode_base64(bytes: &[u8]) -> String {
        const TABLE: &[u8; 64] =
            b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        let mut output = String::new();
        for chunk in bytes.chunks(3) {
            let first = chunk[0];
            let second = *chunk.get(1).unwrap_or(&0);
            let third = *chunk.get(2).unwrap_or(&0);
            output.push(TABLE[(first >> 2) as usize] as char);
            output.push(TABLE[(((first & 3) << 4) | (second >> 4)) as usize] as char);
            output.push(if chunk.len() > 1 {
                TABLE[((second & 15) << 2 | third >> 6) as usize] as char
            } else {
                '='
            });
            output.push(if chunk.len() > 2 {
                TABLE[(third & 63) as usize] as char
            } else {
                '='
            });
        }
        output
    }

    #[test]
    fn imports_minimal_embedded_gltf_fixture() {
        let mut buffer = Vec::new();
        for value in [
            -1.0_f32, -1.0, 0.0, 1.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0,
            0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.5, 1.0,
        ] {
            buffer.extend_from_slice(&value.to_le_bytes());
        }
        buffer.extend_from_slice(&[0, 0, 1, 0, 2, 0]);
        let encoded = encode_base64(&buffer);
        let json = format!(
            r#"{{
                "buffers":[{{"uri":"data:application/octet-stream;base64,{encoded}","byteLength":102}}],
                "bufferViews":[
                    {{"buffer":0,"byteOffset":0,"byteLength":36}},
                    {{"buffer":0,"byteOffset":36,"byteLength":36}},
                    {{"buffer":0,"byteOffset":72,"byteLength":24}},
                    {{"buffer":0,"byteOffset":96,"byteLength":6}}
                ],
                "accessors":[
                    {{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}},
                    {{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"}},
                    {{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}},
                    {{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}}
                ],
                "meshes":[{{"primitives":[{{"attributes":{{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2}},"indices":3}}]}}]
            }}"#
        );
        let path =
            std::env::temp_dir().join(format!("gameengine-minimal-{}.gltf", std::process::id()));
        std::fs::write(&path, json).expect("fixture should be written");
        let assets = import_gltf(&path).expect("fixture should import");
        assert_eq!(assets.len(), 1);
        assert_eq!(
            format::parse(&assets[0].bytes)
                .expect("mesh should parse")
                .kind,
            AssetKind::Mesh
        );
        let _ = std::fs::remove_file(path);
    }
}
