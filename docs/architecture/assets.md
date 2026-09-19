# Asset pipeline

Phase 5 keeps heavy source formats outside the runtime:

```text
glTF / procedural source
        ↓
Rust offline tool
        ↓
.gemesh / .getex / .gemat / .gescene
        ↓
C++ asset reader
        ↓
renderer or scene systems
```

The renderer still uses the procedural Phase 4 bootstrap scene. The C++ reader is an internal validation and view layer for the next integration step; it does not load files, allocate per-asset copies or interpret glTF, PNG or JPG.

## Version 1 container

Each asset has a fixed 64-byte little-endian header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | type magic: `GMSH`, `GTEX`, `GMAT` or `GSCN` |
| 4 | 2 | format version, currently `1` |
| 6 | 2 | header size, currently `64` |
| 8 | 4 | flags, currently little-endian flag `1` only |
| 12 | 8 | complete file size |
| 20 | 8 | deterministic FNV-1a 64 asset ID |
| 28 | 8 | FNV-1a 64 hash of chunk payloads in table order |
| 36 | 4 | chunk count |
| 40 | 4 | chunk entry size, currently `40` |
| 44 | 20 | reserved zero bytes |

Chunk payloads start after a 40-byte entry table and are aligned to 16 bytes. Entries contain the chunk type, flags, file offset, stored size, original size, alignment and reserved bytes. Version 1 has no compression, so stored and original sizes must match. The reader rejects unknown flags, truncation, integer overflow, out-of-file ranges, overlaps, invalid alignment and hash mismatches.

The payload contracts are intentionally small:

- `.gemesh`: position/normal/UV vertices with 32-byte stride, `uint16` or `uint32` indices and submeshes;
- `.getex`: RGBA8 pixels, dimensions and mip count;
- `.gemat`: aligned base color, metallic/roughness values and texture IDs;
- `.gescene`: flat transform instances with mesh and material IDs.

Package output uses the same container rules with `GPAK` magic and deterministic entries sorted by asset ID. Duplicate IDs and missing references are rejected by the offline package step. The package is a tooling artifact; the runtime reader currently exposes the four asset views above.

## Rust workspace

The optional workspace is `tools/Cargo.toml` and contains `gameengine-asset-tool`. It has only `serde` and `serde_json` as direct dependencies. The supported commands are:

```text
asset-tool validate <asset>
asset-tool inspect <asset>
asset-tool import-gltf <input.gltf> --output-dir <dir>
asset-tool pack-mesh
asset-tool pack-texture
asset-tool pack-material
asset-tool pack-scene
asset-tool package --input-dir <dir> --output <package>
```

The glTF importer accepts JSON glTF with inline or external buffers, positions, normals, `TEXCOORD_0`, triangle-list primitives, `uint16`/`uint32` indices and basic metallic-roughness materials. It rejects GLB, Draco, animations, skins, morph targets, unsupported attributes/types and image references. PNG/JPG decoding is intentionally a technical debt item; texture tests use generated RGBA8 memory fixtures.

Incremental work is keyed by source bytes, options and tool version under `build/asset-cache/<source-hash>-<tool-version>/`. Temporary files are used before final replacement, and no binary asset fixtures are committed to the repository.

Format version `1` is an internal development contract. Future incompatible changes must increment the version and retain explicit validation rather than silently interpreting old data.
