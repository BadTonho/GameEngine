use crate::{AssetError, Result};

pub const VERSION: u16 = 1;
pub const HEADER_SIZE: usize = 64;
pub const CHUNK_ENTRY_SIZE: usize = 40;
pub const MAX_CHUNKS: usize = 64;
pub const ALIGNMENT: usize = 16;
pub const FLAG_LITTLE_ENDIAN: u32 = 1;
pub const MAX_FILE_SIZE: usize = 1 << 30;

pub const MESH_MAGIC: [u8; 4] = *b"GMSH";
pub const TEXTURE_MAGIC: [u8; 4] = *b"GTEX";
pub const MATERIAL_MAGIC: [u8; 4] = *b"GMAT";
pub const SCENE_MAGIC: [u8; 4] = *b"GSCN";
pub const PACKAGE_MAGIC: [u8; 4] = *b"GPAK";

pub const CHUNK_MESH_HEADER: u32 = fourcc_const(*b"MSHD");
pub const CHUNK_VERTICES: u32 = fourcc_const(*b"VERT");
pub const CHUNK_INDICES: u32 = fourcc_const(*b"INDX");
pub const CHUNK_SUBMESHES: u32 = fourcc_const(*b"SUBM");
pub const CHUNK_TEXTURE_HEADER: u32 = fourcc_const(*b"TXHD");
pub const CHUNK_TEXTURE_DATA: u32 = fourcc_const(*b"TXDT");
pub const CHUNK_MATERIAL_HEADER: u32 = fourcc_const(*b"MTHD");
pub const CHUNK_SCENE_HEADER: u32 = fourcc_const(*b"SCHD");
pub const CHUNK_INSTANCES: u32 = fourcc_const(*b"INST");
pub const CHUNK_SCENE_ENTITIES: u32 = fourcc_const(*b"ENTS");
pub const CHUNK_SCENE_TRANSFORMS: u32 = fourcc_const(*b"TRNS");
pub const CHUNK_SCENE_MESH_RENDERERS: u32 = fourcc_const(*b"MESH");
pub const CHUNK_SCENE_CAMERAS: u32 = fourcc_const(*b"CAMR");
pub const CHUNK_SCENE_LIGHTS: u32 = fourcc_const(*b"LITE");
pub const CHUNK_PACKAGE_ENTRY: u32 = fourcc_const(*b"PENT");
pub const CHUNK_PACKAGE_DATA: u32 = fourcc_const(*b"PDAT");

pub const fn fourcc_const(value: [u8; 4]) -> u32 {
    (value[0] as u32)
        | ((value[1] as u32) << 8)
        | ((value[2] as u32) << 16)
        | ((value[3] as u32) << 24)
}

pub fn fourcc(value: &[u8; 4]) -> u32 {
    fourcc_const(*value)
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum AssetKind {
    Mesh,
    Texture,
    Material,
    Scene,
    Package,
}

impl AssetKind {
    pub const fn magic(self) -> [u8; 4] {
        match self {
            Self::Mesh => MESH_MAGIC,
            Self::Texture => TEXTURE_MAGIC,
            Self::Material => MATERIAL_MAGIC,
            Self::Scene => SCENE_MAGIC,
            Self::Package => PACKAGE_MAGIC,
        }
    }

    pub fn from_magic(magic: [u8; 4]) -> Option<Self> {
        [
            Self::Mesh,
            Self::Texture,
            Self::Material,
            Self::Scene,
            Self::Package,
        ]
        .into_iter()
        .find(|kind| kind.magic() == magic)
    }

    pub const fn extension(self) -> &'static str {
        match self {
            Self::Mesh => "gemesh",
            Self::Texture => "getex",
            Self::Material => "gemat",
            Self::Scene => "gescene",
            Self::Package => "gepack",
        }
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ChunkInput {
    pub kind: u32,
    pub data: Vec<u8>,
}

#[derive(Clone, Copy, Debug)]
pub struct ChunkView<'a> {
    pub kind: u32,
    pub flags: u32,
    pub offset: usize,
    pub data: &'a [u8],
}

#[derive(Debug)]
pub struct ParsedAsset<'a> {
    pub kind: AssetKind,
    pub version: u16,
    pub asset_id: u64,
    pub content_hash: u64,
    pub chunks: Vec<ChunkView<'a>>,
}

impl<'a> ParsedAsset<'a> {
    pub fn chunk(&self, kind: u32) -> Option<&ChunkView<'a>> {
        self.chunks.iter().find(|chunk| chunk.kind == kind)
    }
}

pub fn fnv1a64(bytes: &[u8]) -> u64 {
    let mut hash = 0xcbf29ce484222325_u64;
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(0x100000001b3_u64);
    }
    hash
}

fn align_up(value: usize, alignment: usize) -> Result<usize> {
    if alignment == 0 || !alignment.is_power_of_two() {
        return Err(AssetError::Invalid("invalid alignment".into()));
    }
    value
        .checked_add(alignment - 1)
        .map(|aligned| aligned & !(alignment - 1))
        .ok_or_else(|| AssetError::Invalid("alignment overflow".into()))
}

fn read_u16(bytes: &[u8], offset: usize) -> Result<u16> {
    let end = offset
        .checked_add(2)
        .ok_or_else(|| AssetError::Invalid("offset overflow".into()))?;
    let value = bytes
        .get(offset..end)
        .ok_or_else(|| AssetError::Invalid("truncated u16".into()))?;
    Ok(u16::from_le_bytes([value[0], value[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Result<u32> {
    let end = offset
        .checked_add(4)
        .ok_or_else(|| AssetError::Invalid("offset overflow".into()))?;
    let value = bytes
        .get(offset..end)
        .ok_or_else(|| AssetError::Invalid("truncated u32".into()))?;
    Ok(u32::from_le_bytes([value[0], value[1], value[2], value[3]]))
}

fn read_u64(bytes: &[u8], offset: usize) -> Result<u64> {
    let end = offset
        .checked_add(8)
        .ok_or_else(|| AssetError::Invalid("offset overflow".into()))?;
    let value = bytes
        .get(offset..end)
        .ok_or_else(|| AssetError::Invalid("truncated u64".into()))?;
    Ok(u64::from_le_bytes(
        value.try_into().expect("checked length"),
    ))
}

fn write_u16(output: &mut [u8], offset: usize, value: u16) {
    output[offset..offset + 2].copy_from_slice(&value.to_le_bytes());
}

fn write_u32(output: &mut [u8], offset: usize, value: u32) {
    output[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
}

fn write_u64(output: &mut [u8], offset: usize, value: u64) {
    output[offset..offset + 8].copy_from_slice(&value.to_le_bytes());
}

pub fn build(kind: AssetKind, chunks: &[ChunkInput]) -> Result<Vec<u8>> {
    if chunks.is_empty() || chunks.len() > MAX_CHUNKS {
        return Err(AssetError::Invalid("invalid chunk count".into()));
    }
    let table_size = chunks
        .len()
        .checked_mul(CHUNK_ENTRY_SIZE)
        .ok_or_else(|| AssetError::Invalid("chunk table overflow".into()))?;
    let mut cursor = align_up(
        HEADER_SIZE
            .checked_add(table_size)
            .ok_or_else(|| AssetError::Invalid("header overflow".into()))?,
        ALIGNMENT,
    )?;
    let mut entries = Vec::with_capacity(chunks.len());
    let mut payload_for_hash = Vec::new();
    for chunk in chunks {
        cursor = align_up(cursor, ALIGNMENT)?;
        let end = cursor
            .checked_add(chunk.data.len())
            .ok_or_else(|| AssetError::Invalid("asset size overflow".into()))?;
        if end > MAX_FILE_SIZE {
            return Err(AssetError::Invalid("asset exceeds maximum size".into()));
        }
        entries.push((chunk.kind, cursor, chunk.data.len()));
        payload_for_hash.extend_from_slice(&chunk.data);
        cursor = end;
    }
    let file_size = cursor;
    let content_hash = fnv1a64(&payload_for_hash);
    let mut identity = Vec::with_capacity(4 + payload_for_hash.len());
    identity.extend_from_slice(&kind.magic());
    identity.extend_from_slice(&payload_for_hash);
    let asset_id = fnv1a64(&identity);
    let mut output = vec![0_u8; file_size];
    output[0..4].copy_from_slice(&kind.magic());
    write_u16(&mut output, 4, VERSION);
    write_u16(&mut output, 6, HEADER_SIZE as u16);
    write_u32(&mut output, 8, FLAG_LITTLE_ENDIAN);
    write_u64(&mut output, 12, file_size as u64);
    write_u64(&mut output, 20, asset_id);
    write_u64(&mut output, 28, content_hash);
    write_u32(&mut output, 36, chunks.len() as u32);
    write_u32(&mut output, 40, CHUNK_ENTRY_SIZE as u32);
    for (index, chunk) in chunks.iter().enumerate() {
        let entry_offset = HEADER_SIZE + index * CHUNK_ENTRY_SIZE;
        let (_, offset, size) = entries[index];
        write_u32(&mut output, entry_offset, chunk.kind);
        write_u32(&mut output, entry_offset + 4, 0);
        write_u64(&mut output, entry_offset + 8, offset as u64);
        write_u64(&mut output, entry_offset + 16, size as u64);
        write_u64(&mut output, entry_offset + 24, size as u64);
        write_u32(&mut output, entry_offset + 32, ALIGNMENT as u32);
        write_u32(&mut output, entry_offset + 36, 0);
        output[offset..offset + size].copy_from_slice(&chunk.data);
    }
    Ok(output)
}

pub fn parse(bytes: &[u8]) -> Result<ParsedAsset<'_>> {
    if bytes.len() < HEADER_SIZE {
        return Err(AssetError::Invalid("asset header is truncated".into()));
    }
    let magic: [u8; 4] = bytes[0..4].try_into().expect("checked length");
    let kind = AssetKind::from_magic(magic)
        .ok_or_else(|| AssetError::Invalid("unknown asset magic".into()))?;
    let version = read_u16(bytes, 4)?;
    if version != VERSION {
        return Err(AssetError::Invalid(format!(
            "unsupported asset version {version}"
        )));
    }
    if usize::from(read_u16(bytes, 6)?) != HEADER_SIZE {
        return Err(AssetError::Invalid("invalid header size".into()));
    }
    if read_u32(bytes, 8)? != FLAG_LITTLE_ENDIAN {
        return Err(AssetError::Invalid("unsupported asset flags".into()));
    }
    if bytes[44..HEADER_SIZE].iter().any(|byte| *byte != 0) {
        return Err(AssetError::Invalid(
            "reserved header bytes are not zero".into(),
        ));
    }
    let file_size = usize::try_from(read_u64(bytes, 12)?)
        .map_err(|_| AssetError::Invalid("file size overflow".into()))?;
    if file_size != bytes.len() || file_size > MAX_FILE_SIZE {
        return Err(AssetError::Invalid("file size does not match asset".into()));
    }
    let asset_id = read_u64(bytes, 20)?;
    let content_hash = read_u64(bytes, 28)?;
    let chunk_count = usize::try_from(read_u32(bytes, 36)?)
        .map_err(|_| AssetError::Invalid("chunk count overflow".into()))?;
    let entry_size = usize::try_from(read_u32(bytes, 40)?)
        .map_err(|_| AssetError::Invalid("entry size overflow".into()))?;
    if chunk_count == 0 || chunk_count > MAX_CHUNKS || entry_size != CHUNK_ENTRY_SIZE {
        return Err(AssetError::Invalid("invalid chunk table".into()));
    }
    let table_end = HEADER_SIZE
        .checked_add(
            chunk_count
                .checked_mul(entry_size)
                .ok_or_else(|| AssetError::Invalid("chunk table overflow".into()))?,
        )
        .ok_or_else(|| AssetError::Invalid("chunk table overflow".into()))?;
    if table_end > bytes.len() {
        return Err(AssetError::Invalid("chunk table is truncated".into()));
    }
    let mut chunks = Vec::with_capacity(chunk_count);
    let mut ranges: Vec<(usize, usize)> = Vec::with_capacity(chunk_count);
    let mut payload_for_hash = Vec::new();
    for index in 0..chunk_count {
        let entry = HEADER_SIZE + index * entry_size;
        let kind = read_u32(bytes, entry)?;
        let flags = read_u32(bytes, entry + 4)?;
        let offset = usize::try_from(read_u64(bytes, entry + 8)?)
            .map_err(|_| AssetError::Invalid("chunk offset overflow".into()))?;
        let size = usize::try_from(read_u64(bytes, entry + 16)?)
            .map_err(|_| AssetError::Invalid("chunk size overflow".into()))?;
        let uncompressed_size = usize::try_from(read_u64(bytes, entry + 24)?)
            .map_err(|_| AssetError::Invalid("chunk size overflow".into()))?;
        let alignment = usize::try_from(read_u32(bytes, entry + 32)?)
            .map_err(|_| AssetError::Invalid("chunk alignment overflow".into()))?;
        if flags != 0
            || read_u32(bytes, entry + 36)? != 0
            || alignment != ALIGNMENT
            || size != uncompressed_size
            || offset % alignment != 0
        {
            return Err(AssetError::Invalid(
                "invalid chunk flags, alignment or compression".into(),
            ));
        }
        let end = offset
            .checked_add(size)
            .ok_or_else(|| AssetError::Invalid("chunk range overflow".into()))?;
        if offset < align_up(table_end, ALIGNMENT)? || end > bytes.len() {
            return Err(AssetError::Invalid("chunk is outside the payload".into()));
        }
        if ranges
            .iter()
            .any(|(start, previous_end)| offset < *previous_end && *start < end)
        {
            return Err(AssetError::Invalid("chunks overlap".into()));
        }
        ranges.push((offset, end));
        let data = &bytes[offset..end];
        payload_for_hash.extend_from_slice(data);
        chunks.push(ChunkView {
            kind,
            flags,
            offset,
            data,
        });
    }
    if fnv1a64(&payload_for_hash) != content_hash {
        return Err(AssetError::Invalid("content hash mismatch".into()));
    }
    let mut identity = Vec::with_capacity(4 + payload_for_hash.len());
    identity.extend_from_slice(&magic);
    identity.extend_from_slice(&payload_for_hash);
    if fnv1a64(&identity) != asset_id {
        return Err(AssetError::Invalid("asset id mismatch".into()));
    }
    Ok(ParsedAsset {
        kind,
        version,
        asset_id,
        content_hash,
        chunks,
    })
}

pub fn read_u32_public(bytes: &[u8], offset: usize) -> Result<u32> {
    read_u32(bytes, offset)
}

pub fn read_u64_public(bytes: &[u8], offset: usize) -> Result<u64> {
    read_u64(bytes, offset)
}

pub fn write_u32_public(output: &mut [u8], offset: usize, value: u32) {
    write_u32(output, offset, value);
}

pub fn write_u64_public(output: &mut [u8], offset: usize, value: u64) {
    write_u64(output, offset, value);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_is_deterministic() {
        let chunks = vec![
            ChunkInput {
                kind: CHUNK_MESH_HEADER,
                data: vec![1, 2, 3],
            },
            ChunkInput {
                kind: CHUNK_VERTICES,
                data: vec![4, 5, 6, 7],
            },
        ];
        let first = build(AssetKind::Mesh, &chunks).expect("build should succeed");
        let second = build(AssetKind::Mesh, &chunks).expect("build should succeed");
        assert_eq!(first, second);
        let parsed = parse(&first).expect("parse should succeed");
        assert_eq!(parsed.kind, AssetKind::Mesh);
        assert_eq!(parsed.chunks.len(), 2);
        assert_eq!(
            parsed.chunk(CHUNK_VERTICES).expect("vertex chunk").data,
            [4, 5, 6, 7]
        );
    }

    #[test]
    fn rejects_truncation_flags_and_overlapping_chunks() {
        let source = build(
            AssetKind::Texture,
            &[
                ChunkInput {
                    kind: CHUNK_TEXTURE_HEADER,
                    data: vec![1, 2, 3, 4],
                },
                ChunkInput {
                    kind: CHUNK_TEXTURE_DATA,
                    data: vec![5, 6, 7, 8],
                },
            ],
        )
        .expect("build should succeed");
        assert!(parse(&source[..source.len() - 1]).is_err());

        let mut invalid_flags = source.clone();
        write_u32(&mut invalid_flags, 8, FLAG_LITTLE_ENDIAN | 2);
        assert!(parse(&invalid_flags).is_err());

        let mut overlap = source.clone();
        let entry_offset = HEADER_SIZE + CHUNK_ENTRY_SIZE;
        overlap.resize(overlap.len() + ALIGNMENT, 0);
        let first_offset = read_u64(&overlap, HEADER_SIZE + 8).expect("offset should be readable");
        write_u64(&mut overlap, entry_offset + 8, first_offset);
        assert!(parse(&overlap).is_err());
    }

    #[test]
    fn rejects_reserved_bytes_and_unknown_version() {
        let source = build(
            AssetKind::Material,
            &[ChunkInput {
                kind: CHUNK_MATERIAL_HEADER,
                data: vec![0; 64],
            }],
        )
        .expect("build should succeed");
        let mut reserved = source.clone();
        reserved[44] = 1;
        assert!(parse(&reserved).is_err());
        let mut version = source;
        write_u16(&mut version, 4, VERSION + 1);
        assert!(parse(&version).is_err());
    }
}
