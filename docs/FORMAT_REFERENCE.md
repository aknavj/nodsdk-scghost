# StarCraft: Ghost — Asset Format Reference

> Reverse-engineered binary and text formats from the unreleased Xbox build of StarCraft: Ghost (Nihilistic/Swingin' Ape Studios, ~2004).
> All binary formats are **little-endian**. Coordinate system is **D3D left-handed Z-up** (X-right, Y-forward, Z-up).

---

## Table of Contents

| # | Format | Extension(s) | Type | Description |
|---|--------|-------------|------|-------------|
| 1 | [DDS](#1-dds--directdraw-surface) | `.dds` | Binary | Standard DirectDraw Surface textures |
| 2 | [TGA](#2-tga--targa-image) | `.tga` | Binary | Targa images (uncompressed + RLE) |
| 3 | [XPR](#3-xpr--xbox-packed-resource) | `.xpr` | Binary | Xbox packed texture archives |
| 4 | [NAD](#4-nad--nihilistic-animation-data) | `.nad` | Binary | Skeletal animation clips |
| 5 | [NOD](#5-nod--nihilistic-object-data) | `.nod` | Binary | 3D models with skeleton + materials |
| 6 | [NMS](#6-nms--nihilistic-motion-set) | `.nms` | Text | Motion name → animation file mapping |
| 7 | [NRT/NUT](#7-nrtnut--nihilistic-terrain) | `.nrt` `.nut` | Binary | Terrain heightmap/patch data |
| 8 | [NIL](#8-nil--nihilistic-indoor-level) | `.nil` | Binary | Indoor level geometry + portals + lightmaps |
| 9 | [NLFX](#9-nlfx--nihilistic-level-effects) | `.nlfx` | Text | Per-level effect reference list |
| 10 | [NPD](#10-npd--nihilistic-patrol-data) | `.npd` | Binary | AI navigation/patrol waypoint graph |
| 11 | [NSA](#11-nsa--nihilistic-shader-archive) | `.nsa` | Text | Material/shader property definitions |
| 12 | [NSD](#12-nsd--nihilistic-scene-data) | `.nsd` | Binary | Scene entity type definitions |
| 13 | [NLT](#13-nlt--nihilistic-localization-text) | `.nlt` | Text | Raw localization text |
| 14 | [NOC](#14-noc--nihilistic-object-collision) | `.noc` | Binary | Per-model collision meshes |
| 15 | [NLS/NLU](#15-nlsnlu--nihilistic-localization-strings) | `.nls` `.nlu` | Text | Key-value localization strings |
| 16 | [NOT](#16-not--nihilistic-object-templates) | `.not` | Binary | Entity/object template database |
| 17 | [NFX](#17-nfx--nihilistic-effects) | `.nfx` | Text | Particle/VFX controller definitions |
| 18 | [NUI](#18-nui--nihilistic-ui-layout) | `.nui` | Text | UI screen layout definitions |
| 19 | [NAK](#19-nak--nihilistic-animation-keyframes) | `.nak` | Text | Camera animation keyframes |
| 20 | [NCE](#20-nce--nihilistic-cutscene-events) | `.nce` | Binary | Cutscene event data |
| 21 | [NCS](#21-ncs--nihilistic-cutscene-script) | `.ncs` | Binary | Cutscene shot/camera scripts |
| 22 | [NMB](#22-nmb--nihilistic-model-bundle) | `.nmb` | Binary | Multi-LOD model bundles |
| 23 | [NNB](#23-nnb--nihilistic-animation-bundle) | `.nnb` | Binary | Animation bundles (paired with NMB) |

---

## 1. DDS — DirectDraw Surface

**Extension:** `.dds`
**Type:** Binary (standard Microsoft DDS format)
**Magic:** `0x20534444` ("DDS ")

Standard DirectDraw Surface container used as the primary texture format. Supports DXT1 (BC1), DXT5 (BC3) compressed formats and uncompressed A8R8G8B8 (BGRA).

### Header (128 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | magic | `0x20534444` ("DDS ") |
| 0x04 | 4 | size | Header size (124) |
| 0x08 | 4 | flags | Content flags |
| 0x0C | 4 | height | Texture height in pixels |
| 0x10 | 4 | width | Texture width in pixels |
| 0x14 | 4 | pitchOrLinearSize | Row pitch or compressed size |
| 0x18 | 4 | depth | Volume depth (usually 0) |
| 0x1C | 4 | mipMapCount | Number of mipmap levels |
| 0x20 | 44 | reserved | Unused |
| 0x4C | 4 | pixelFormatSize | Pixel format struct size (32) |
| 0x50 | 4 | pixelFormatFlags | Pixel format flags |
| 0x54 | 4 | fourCC | Compression format (`DXT1`, `DXT5`, etc.) |
| 0x58 | 4 | rgbBitCount | Bits per pixel (uncompressed) |
| 0x5C | 16 | bitMasks | R, G, B, A channel masks |
| 0x6C | 4 | caps1 | Surface complexity flags |
| 0x70 | 4 | caps2 | Additional surface flags |
| 0x74 | 12 | reserved2 | Unused |

Pixel data follows immediately after the header. Mipmap chain is stored consecutively, each level half the previous dimensions.

---

## 2. TGA — Targa Image

**Extension:** `.tga`
**Type:** Binary (standard TGA format)
**Magic:** None (no signature; validated via header fields)

Standard Targa format. Supports uncompressed and RLE-compressed true-color images.

### Header (18 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 1 | idLength | Image ID field length |
| 0x01 | 1 | colorMapType | 0=none, 1=present |
| 0x02 | 1 | imageType | 2=uncompressed, 10=RLE |
| 0x03 | 5 | colorMapSpec | First entry, length, bit depth |
| 0x08 | 10 | imageSpec | Origin X/Y, width, height, bpp, descriptor |

**Supported image types:** Uncompressed RGB (type 2), RLE RGB (type 10).
**Bit depths:** 24-bit (BGR) and 32-bit (BGRA).

---

## 3. XPR — Xbox Packed Resource

**Extension:** `.xpr`
**Type:** Binary
**Magic:** `0x30525058` ("XPR0")

Xbox-specific packed texture archive. Contains multiple named texture resources, each with a 20-byte D3D descriptor that encodes format, dimensions, and mipmap info in Xbox GPU-native format.

### File Layout

```
[Header: 16 bytes]
  uint32 magic         "XPR0"
  uint32 totalSize     Total file size
  uint32 headerSize    Offset to pixel data
  uint32 resourceCount Number of resources

[Resource Table: N × 8 bytes]
  uint32 nameOffset    Offset to null-terminated name string
  uint32 descriptorOff Offset to 20-byte D3D texture descriptor

[Name Strings: variable]
  Null-terminated ASCII strings

[Descriptors: N × 20 bytes]
  Opaque Xbox D3D texture descriptor

[Texture Data: bulk]
  Raw pixel data (DXT1/DXT5/A8R8G8B8)
```

The D3D descriptor encodes: format (DXT1/DXT3/DXT5/A8R8G8B8/L8/A8L8/etc.), width, height, mipmap count, and data offset within the texture data section.

---

## 4. NAD — Nihilistic Animation Data

**Extension:** `.nad`
**Type:** Binary
**Version:** v4

Skeletal animation clip format. Bone tracks store keyframes with cubic polynomial coefficients for smooth interpolation.

### File Layout

```
[Header: 16 bytes]
  uint32 version        Always 4
  uint32 numBoneTracks  Number of bone animation tracks
  uint32 flags          Animation flags
  float  duration       Duration in frames (at 30 fps)

[Bone Tracks: N entries]
  Per-track header (8 bytes):
    uint32 boneNumber   Index into skeleton
    uint16 type         0=rotation (euler°), 1=position
    uint16 numKeys      Keyframe count

  Followed by numKeys × Keyframe (52 bytes each):
    float  frame        Time in frames
    vec3   value        Base value (euler degrees or position)
    vec3   bCoeff       Linear coefficient
    vec3   cCoeff       Quadratic coefficient
    vec3   dCoeff       Cubic coefficient

[Key Tags: variable]
  uint32 numKeyTags
  Per tag (8 bytes):
    uint32 tagType      Event type enum (eRenderKeyTag)
    float  frameNum     Frame at which event fires

[Shape Tracks: variable]
  uint32 numShapeTracks
  Per shape track:
    uint8  groupNum, shapeNum
    uint16 numKeys
    Per key (8 bytes):
      float frame, weight
```

### Keyframe Evaluation

Animation values at time $t$ are evaluated using a cubic polynomial:

$$result = value + t \cdot b + t^2 \cdot c + t^3 \cdot d$$

where $t$ is the normalized time within the keyframe interval.

---

## 5. NOD — Nihilistic Object Data

**Extension:** `.nod`
**Type:** Binary
**Version:** `flags` field ≥ 10

3D model format with skeleton, multiple vertex types, indexed triangle meshes, and material references.

### File Layout

```
[Header: 92 bytes (0x5C)]
  uint32  flags          Version/feature flags (≥ 10)
  uint8   shaderCount    Number of material names
  uint8   boneCount      Skeleton bone count
  uint8   vertexSetCount Number of vertex buffer sets
  uint8   meshGroupCount Number of draw groups
  uint32  unknown
  vec3    boundsMin      AABB minimum
  vec3    boundsMax      AABB maximum
  ...     (padding/reserved to 92 bytes)
  uint32  indexCount     Total index count

[Shader Names: shaderCount × 32 bytes]
  Null-padded material name strings

[Bones: boneCount × 64 bytes]
  float[3]  position     Rest-pose translation
  float[9]  orientation  3×3 rotation matrix (row-major)
  float[3]  offset       Inverse root translation
  uint8     pad0         Secondary link / IK parent
  uint8     pad1         Joint mapping tag
  uint8     parent       Parent bone index (0xFF = root)
  uint8     jointId      Joint identifier

[Vertex Data: variable]
  Per vertex set: count × stride bytes
  Stride determined by vertex type

[Index Buffer: indexCount × uint16]
  Triangle strip and list indices

[Mesh Groups: meshGroupCount × 56 bytes]
  uint32  shaderIndex    Material index
  Strip/list index ranges
  uint32  baseVertex     Base vertex offset
  uint8   vertexSetIndex Which vertex set to use
  uint8   numBones       Bone palette size
  uint8[20] aBones       Bone palette indices
```

### Vertex Types

| Type | Total bytes | Layout | Description |
|------|-------------|--------|-------------|
| 0 | 32 | pos(12) + normal(12) + uv(8) | Rigid |
| 1 | 36 | pos(12) + normal(12) + uv(8) + boneIdx(4) | Single-bone skinned |
| 2 | 48 | pos(12) + normal(12) + uv(8) + weights(12) + boneIdx(4) | Weighted skinned |
| 3 | 68 | [stream 0: 32B = type 0] + [stream 1: 36B TBN] | Bump-mapped rigid |
| 4 | 72 | [stream 0: 36B = type 1] + [stream 1: 36B TBN] | Bump-mapped single-bone |
| 5 | 84 | [stream 0: 48B = type 2] + [stream 1: 36B TBN] | Bump-mapped weighted |

**Sequential stream layout (types 3–5):** Types 3–5 store vertex data as **two sequential (non-interleaved) streams**:
- **Stream 0:** `count × baseStride` bytes — identical layout to the base type (3→0, 4→1, 5→2)
- **Stream 1:** `count × 36` bytes — TBN data: tangent(12) + binormal(12) + normal(12), all unit vectors

The total byte count per vertex set is `count × totalStride`, but stream 0 vertex data must be read at the base stride (32/36/48), not the combined stride.

**Type prevalence:** Type 0 (1379 files), Type 1 (154), Type 2 (102), Type 3 (11), Type 4 (3), Type 5 (1 — lurker.nod).

---

## 6. NMS — Nihilistic Motion Set

**Extension:** `.nms`
**Type:** Text (tab-delimited)

Maps motion state names to animation (.nad) file paths. Used by the animation system to resolve gameplay states to specific clips.

### Format

```
; Semicolon-prefixed comment header
motionName<TAB>animFile.nad<TAB>[flag1]<TAB>[flag2]...
```

Each line maps a motion name (e.g., `idle`, `run`, `attack_01`) to a `.nad` animation file, optionally with flags that control playback behavior.

---

## 7. NRT/NUT — Nihilistic Terrain

**Extension:** `.nrt`, `.nut`
**Type:** Binary

Terrain heightmap and texture data organized as a grid of patches.

### File Layout

```
[Header: 8 bytes]
  uint32  version
  uint16  gridWidth
  uint16  gridHeight

[Patches: variable]
  Per patch:
    char[256]  name           Patch/texture name
    float[4]   scaleMatrix    2×2 transform matrix
    uint32     dataSize       Raw patch data size
    uint8[]    data           Heightmap/blend data
```

The `.nrt` file contains the geometry/heightmap data while `.nut` contains the texture layer assignments. Both share the same grid dimensions.

---

## 8. NIL — Nihilistic Indoor Level

**Extension:** `.nil`
**Type:** Binary
**Magic:** `0x104C494E` ("NIL\x10")
**Version:** 35

The primary level geometry format. Contains sector-based BSP/portal architecture with per-vertex baked lighting, lightmap patches, and environment map references.

### File Layout

```
[Header]
  uint32  magic            "NIL\x10"
  uint32  version          35
  uint8[72] globalLight    Global light parameters
  float[3]  ambientColor   RGB ambient color
  uint32  numShaders       Material count
  char[]  shaderNames      numShaders × 33-byte padded names
  uint16  numEnvMaps       Environment map count
  char[]  envMapNames      numEnvMaps × 33-byte padded names
  uint32  numSectors       Sector count

[Sectors: numSectors entries]
  Per sector (variable size):
    uint32    flags
    float[3]  center       Sector center point
    uint8     numPortals   Portal count
    uint8[N]  portalData   Portal definitions (N × 16 bytes)
    uint8     hasGeo       0=empty, 1=standard, 2=with lightmaps
    uint8     vertType     0=standard, 1=with TBN, 2=vertex color
    uint16    numVerts     Vertex count

    [Vertex Data: numVerts × 36 bytes]
      float[3]  position   World-space position
      float[3]  normal     Surface normal
      uint32    diffuse    BGRA packed vertex color (Xbox D3DCOLOR)
      float[2]  uv         Texture coordinates

    [TBN Data: numVerts × 36 bytes, if vertType == 1]
      float[3]  tangent
      float[3]  binormal
      float[3]  normal

    [Lightmap Data: if lmFlag set]
      uint32  lmPatchCount
      ...patch data...

    uint16    numGroups    Surface group count
    [Surface Groups: numGroups entries, 47 bytes each]
      uint32  shaderIdx        Material index
      uint32  startIdx         Index buffer start
      uint32  field8
      uint16  numStripIndices  Triangle strip index count
      uint16  numListIndices   Triangle list index count
      uint32  field16
      uint8   vertType         Vertex type override
      uint8   flag             Rendering flags
      uint8   field22
      float[3] bboxMin         Group AABB minimum
      float[3] bboxMax         Group AABB maximum

    uint32    numIndices   Index count
    uint16[]  indices      Triangle strip/list indices

    [Things: variable]
      uint8   numThings
      [Thing: 68 bytes each]
        float[3]  position
        uint32    field1
        uint32    field2
        float[12] matrix    4×vec3 row matrix

    [SurfThings: variable]
      uint16  numSurfThings
      [SurfThing: 72 bytes each]
        uint8[72] data      Opaque

    [EnvRefs: variable]
      uint16  numEnvRefs
      char[]  envRefNames   N × 33-byte names

    [Tail: 6 bytes]
      uint16  tailField1   Unknown
      uint32  tailField2   Unknown

[Post-Sector Data: variable size, ~11–42% of file]
  The data after the sector loop contains spatial/lighting structures
  that have NOT been fully decoded. Summary from hex analysis:

  [Section 1: Portal Collision Mesh]
    uint32       marker      Always 0
    float[6]     aabb        Level bounding box (min xyz, max xyz), LE
    uint8        type        Always 1
    uint8        pad         Always 0
    uint8        flag        Always 1
    uint8        vertCount   Number of collision vertices
    float[3][]   vertices    vertCount × 12 bytes, LE float3 positions
                             Forms axis-aligned box faces (portal boundaries)
    uint16       tail        Unknown (e.g. 0x90C0)

  [Section 2: Triangle Indices]
    uint8[~5]    zeros       Section separator
    uint16[]     indices     Triangle indices into Section 1 vertices
                             Pattern: (2,1,0), (3,2,4), (6,5,4), ...

  [Section 3+: BSP Tree / Lightmap Atlas — NOT DECODED]
    ~99% of the post-sector data. Byte entropy 7.0–7.3 / 8.0
    indicates compressed texture data, likely the lightmap atlas.
    Contains:
    - Scattered float3 positions (world coordinates)
    - AABB-like patterns throughout (~200+ in first 100KB)
    - Possible BSP tree node data (splitting planes + child refs)
    - Dense high-entropy regions (lightmap pixel data)

  [Tail: Offset Table]
    Last ~500–600 bytes of the file.
    Pairs of (uint32 offset, uint32 flag) where flag ∈ {0, 1}.
    Values are monotonically increasing within groups.
    Groups separated by flag=0 entries.
    Likely sector-indexed offsets into the lightmap/BSP data.
```

### Per-File Post-Sector Statistics

| File | Sectors | Remaining | % of File |
|------|---------|-----------|-----------|
| 1_2_1_Miners_Bunker.nil | 14 | 973 KB | 30.1% |
| 1_2_1a_Miners_Bunker.nil | 40 | 1,043 KB | 41.9% |
| 1_2_1b_Miners_Bunker.nil | 55 | 1,281 KB | 30.4% |
| 1_3_1a_Hive_StationOuter.nil | 47 | 1,537 KB | 27.6% |
| 1_3_1b_Hive_StationOuter.nil | 302 | 2,046 KB | 29.9% |
| 2_2_2_THREAT_BASEUPPER.nil | 43 | 209 KB | 11.0% |
| 2_2_2a_Threat_BaseUpper.nil | 116 | 509 KB | 12.3% |
| menu.nil | 2 | 790 KB | 34.1% |

### Vertex Color Encoding

Xbox D3DCOLOR is BGRA packed as `uint32`. Vertex colors encode baked lighting with Xbox overbright convention: value 128 = 1.0 intensity, 255 ≈ 2.0 intensity. The renderer scales by `color / 128.0`.

---

## 9. NLFX — Nihilistic Level Effects

**Extension:** `.nlfx`
**Type:** Text

Simple list of effect names referenced by a level, one per line. These names correspond to controllers defined in the global `.nfx` effect files.

---

## 10. NPD — Nihilistic Patrol Data

**Extension:** `.npd`
**Type:** Binary

AI navigation/patrol waypoint graph for a level. Contains waypoint nodes with
positions in navigation-map coordinate space (NOT world-space) and an adjacency
list defining connectivity between waypoints for AI pathfinding.

**Important:** NPD is NOT entity placement data. Entity world-space positions
are defined in the NSD file. NPD positions are in a separate 2D navigation-map
coordinate space with a compressed Z axis (floor/elevation index). The
`waypointId` field is a unique waypoint identifier, not an NSD entity reference.

### Header (8 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 2 | version | Format version (= 2) |
| 0x02 | 2 | nodeCount | Number of valid waypoint records |
| 0x04 | 4 | unknown | Reserved / unknown |

### Waypoint Records (nodeCount × 26 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 2 | waypointId | Unique waypoint identifier (u16) |
| 0x02 | 12 | position | Navigation-map position (3 floats, XYZ) |
| 0x0E | 2 | flags | Waypoint flags (0=normal, 1=special, 1024=?) |
| 0x10 | 4 | color | Debug visualization color (ARGB) |
| 0x14 | 6 | extra | Additional data (3 × u16) |

### Adjacency List (remaining bytes after records)

After the waypoint records, the rest of the file is an adjacency list defining
connections between waypoints. Each entry is 6 bytes:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 2 | fromNode | Source node index (0-based record index) |
| 0x02 | 2 | toNode | Destination node index (0-based record index) |
| 0x04 | 2 | cost | Edge cost/weight (0, 1, 2, 3, or special values) |

The adjacency list uses **0-based record indices** (not waypointIds). Typical
levels have ~3–4 links per node, forming 1–3 connected components.

### Coordinate Space

NPD positions use a navigation-map coordinate system distinct from NSD
world-space:
- X/Y axes cover approximately the same spatial extent as NSD but at ~4× scale
- Z axis is compressed to floor/elevation levels (e.g., -10.9, -7.6, 1.2, 2.9, 9.9)
- Not directly usable for entity rendering; only for AI pathfinding logic

---

## 11. NSA — Nihilistic Shader Archive

**Extension:** `.nsa`
**Type:** Text (Quake 3 Arena shader style)

Material property definitions in a Quake 3 style text format. Defines texture references, blend modes, shader types, and rendering parameters for level and model materials.

### Format

```
MaterialName
{
    key1 value1
    key2 value2
    ...
}
```

Common properties include: `diffuseMap`, `glowMap`, `detailMap`, `envMap`, `shaderType`, `blendMode`, `alphaTest`, `intensity`, `detailScale`.

---

## 12. NSD — Nihilistic Scene Data

**Extension:** `.nsd`
**Type:** Binary

Scene entity database for a level. Contains entity type definitions and their
instance data (positions, rotations, properties). This is the **authoritative
source** for entity world-space positions. Used to extract entity spawn points
including the player "ghost" entity.

### Header (92 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | magic | `NSD\x10` (0x1044534E) |
| 0x04 | 4 | entityCount | Header entity count (cutscene entities?) |
| 0x08 | 72 | metadata | Level metadata / ambient data |
| 0x50 | 4 | totalEntities | Total entity count from scanner (at offset 80) |
| 0x54 | 4 | sectorCount | Number of sectors/rooms (at offset 84) |
| 0x58 | 4 | reserved | Zero padding |

### Entity Block Layout

Entity blocks begin at offset 92 and are variable-length. Each entity uses a
tag-based property system with 36-byte property slots:

```
[Type Name: 32 bytes]
  Null-terminated ASCII string, zero-padded to 32 bytes.

[Property Block 0 (rotation): 36 bytes at T+32]
  uint32  tag1        = 1 (rotation tag)
  float[3] rotation   Euler angles (pitch, yaw, roll in degrees)
  byte[20] filler     Leftover name bytes / padding

[Property Block 1 (position): 36 bytes at T+68]
  uint32  tag2        = 2 (position tag)
  float[3] position   World-space position (D3D Z-up coordinates)
  byte[20] filler     Leftover name bytes / padding

[Property Block 2 (sector): 36 bytes at T+104]
  uint32  tag3        = 3 (model entity) or 15 (region entity)
  uint32  sectorId    Room/sector index (groups entities by area)
  float[2] posYZ      Duplicate of position Y, Z (stale data)
  byte[20] filler     Leftover name bytes / padding

[Property Block 3+ (optional): 36 bytes at T+140]
  uint32  tag4        = 4 (model name) or 0x2E (46, script ref) etc.
  char[]  value       Model filename or script name (null-terminated)
  ...                 Additional property blocks follow in 36-byte steps
```

### Tag Values

| Tag | Meaning | Data |
|-----|---------|------|
| 1 | Rotation | 3 floats: pitch, yaw, roll (degrees) |
| 2 | Position | 3 floats: X, Y, Z (D3D world-space) |
| 3 | Model entity | sector ID (u32), then stale position data |
| 4 | Model name | Null-terminated string (e.g., "door.nod") |
| 15 | Region class | Region class name (e.g., "cRegionSound") |
| 46 (0x2E) | Script ref | Script/animation name string |

### Entity Type Categories

Entity `typeName` values classify entities:
- **Model entities** (tag3=3): `instance_nocfile`, `ge_door*`, `movetrack_*`, `movenone_*`, etc.
- **Region entities** (tag3=15): `trigger_region`, `camera_region`, `alarm_region`, `norain_region`
- **Character entities**: `ghost` (player), `scvdriver*`, `infantry_hauler*`, `hydralisk*`, etc.
- **Pickup entities**: `powhealth`, `powenergy`, `powgrenade`, `powdata`, etc.
- **Skip types** (non-visual): `script_object`, `halo_*`, `cone_*`, `sparkfountain`, etc.

---

## 13. NLT — Nihilistic Localization Text

**Extension:** `.nlt`
**Type:** Plain text

Raw localization text file. Contains unstructured text content (dialogue, UI strings, etc.).

---

## 14. NOC — Nihilistic Object Collision

**Extension:** `.noc`
**Type:** Binary
**Version:** 10

Per-model collision mesh. Contains simplified geometry used for physics/collision detection.

### File Layout

```
[Header: 88 bytes (0x58)]
  uint32  version      Always 10
  uint8[4] flags       flags[0]=name mode, flags[3]=shape count
  vec3    boundsMin    AABB minimum
  vec3    boundsMax    AABB maximum
  ...     (additional header fields)

[Shape Descriptor: 100 or 132 bytes]
  Depends on flags[0]:
    flags[0]=1: 100 bytes (name only)
      4B pad + 32B name + 12B pad + 36B matrix + 12B position + 4B color
    flags[0]=2: 132 bytes (name + collision type)
      4B pad + 32B shapeName + 32B collisionType + 12B pad + 36B matrix + 12B position + 4B color

[Vertex Data: numVertices × 32 bytes]
  float[3]  position   Vertex position
  float[3]  normal     Vertex normal
  float[2]  uv         Texture coordinates

[Index Data: numIndices × uint16]
  Triangle list indices

[Footer: 64 + (shapeCount-1) × 56 bytes]
  Additional shape descriptors
```

---

## 15. NLS/NLU — Nihilistic Localization Strings

**Extension:** `.nls`, `.nlu`
**Type:** Text (key-value)

Localized string tables mapping string identifiers to display text.

### Format

```
# Comment line
STRING_KEY    The displayed text value
ANOTHER_KEY   Another string value
```

Lines beginning with `#` are comments. Keys and values are separated by whitespace. The parser provides case-insensitive key lookup via `LookupString()`.

---

## 16. NOT — Nihilistic Object Templates

**Extension:** `.not`
**Type:** Binary
**Magic:** `0x10544F4E` ("NOT\x10")
**Version:** 1

Entity template database defining the property hierarchy for all game object types. Contains ~1224 templates organized in an inheritance tree rooted at 8 base classes.

### File Layout

```
[Header: 12 bytes]
  uint32  magic          "NOT\x10"
  uint32  version        Always 1
  uint32  numTemplates   Template count

[Table of Contents: numTemplates × 36 bytes]
  char[32] name          Template name
  uint32   dataOffset    Offset to template data

[Template Data: variable]
  Per template:
    uint32  flags         47=root, 0=child
    char[32] parent       Parent template name
    uint32  numProperties Property count

    [Properties: variable]
      Three encoding types:
        Standard (36 bytes): uint32 propID + uint8[32] value
        Boolean  (5 bytes):  uint32 propID(=1) + uint8 value
        Comment  (variable): uint32 strLen + char[] text
```

### Root Templates (flags=47)

| Name | Description |
|------|-------------|
| `_ACTOR` | Characters, NPCs, enemies |
| `_EFFECT` | Visual/audio effects |
| `_PROJECTILE` | Weapons projectiles |
| `_PROP` | Static/dynamic props |
| `_REGION` | Trigger regions/volumes |
| `_SENSOR` | Detection sensors |
| `_UIELEMENT` | HUD/UI elements |

All other templates inherit from one of these roots.

---

## 17. NFX — Nihilistic Effects

**Extension:** `.nfx`
**Type:** Text (block-structured)

Particle system and visual effects definitions. The main `game.nfx` contains ~1100+ effect blocks and ~144 controllers.

### Format

```
; Semicolon comment
<blockType> <name>
    property1 value1
    property2 value2
end
```

### Block Types

`controller`, `smoke`, `sparks`, `glow`, `light`, `shockwave`, `spikes`, `model`, `fxmodel`, `decal`, `trail`, `billboard`, `damage`, `ctrldamage`, `force`, `spawner`, `mushroom`, `volumefog`, `energybeam`, `rain`, `muzzle_flash`, `exp_fire`, `project`, `projectcube`

---

## 18. NUI — Nihilistic UI Layout

**Extension:** `.nui`
**Type:** Text

UI screen layout definitions. 5 files exist: `console`, `debug`, `LoadScreen`, `mainMenu`, `TitleScreen`.

### Format

```
; Comment
name=ScreenName
pos=x,y
dialog=1
music=soundName

element <type> name=... pos=x,y size=w,h color=r,g,b,a font=...
quad name=... pos=x,y size=w,h texture=...
```

Element types define buttons, text fields, images, and other UI widgets.

---

## 19. NAK — Nihilistic Animation Keyframes

**Extension:** `.nak`
**Type:** Text

Camera animation keyframes paired with cutscene `.nad` files (3 files total).

### Format

```
<keyframeCount>
<time> <fov> <nearClip> <farClip>
<time> <fov> <nearClip> <farClip>
...
```

First line is the keyframe count, followed by one keyframe per line with four float values: timestamp, field of view, near clip plane, and far clip plane distances.

---

## 20. NCE — Nihilistic Cutscene Events

**Extension:** `.nce`
**Type:** Binary
**Version:** 1

Cutscene event data defining actors, animations, camera, sound cues, and conversation triggers for cinematics. 207 files in `Cutscenes/`, each paired 1:1 with a `.ncs` script file. Validated: 207/207 files parse successfully.

### File Layout

```
[Header]
  +0   uint32  version         Always 1
  +4   uint8   unknownFlag     Single flag byte
  +5   uint32  mapNameLen      Map name string length
  +9   char[]  mapName         Map name (mapNameLen bytes, null-padded)

[Timing Block: 40 bytes, at offset 9 + mapNameLen]
  +0   uint32  numFrames       Total frame count (e.g. 38)
  +4   float   timeScale       Playback time scale (1.0)
  +8   ...     reserved        18 bytes
  +26  uint16  shotCount       Number of camera shots
  +28  ...     reserved        6 bytes
  +34  uint16  eventCount      Number of scripted events

[Camera Base Block: 111 bytes]
  +0   ...     config          67 bytes of camera config flags
  +67  float   posX            Camera initial position X
  +71  float   posY            Camera initial position Y
  +75  float   posZ            Camera initial position Z
  +79  float   reserved        Unused (0)
  +83  float   fov             Field of view in degrees (typically 50–80)
  +87  float   reserved        Unused (0)
  +91  float   targetX         Camera look-at target X
  +95  float   targetY         Camera look-at target Y
  +99  float   targetZ         Camera look-at target Z
  +103 ...     padding         8 bytes

[Camera NAD: optional 32 bytes at camera+111]
  char[32] cameraNAD           External camera animation path (e.g. "1_2_1a_tankRush_camera.nad")
                               Validated by checking for ".nad" suffix (len > 8)
                               23 of 207 files have external camera NAD

[Actor Records: variable, each preceded by 8-byte marker]
  Marker pattern: uint32(1), uint32(1) — searched via memmem()
  Validated by checking that byte at marker+8 is printable ASCII (0x20..0x7E)

  Per actor (140 bytes base):
    +0   uint32  marker1        Always 1
    +4   uint32  marker2        Always 1
    +8   char[32] modelName     Template model name (e.g. "nova_cut")
    +40  char[32] instanceName  Instance name (e.g. "Nova_121a_tankRush")
    +72  uint8[23] flags        Flag bytes:
         [2] visibility         0x00=hidden, 0x01=visible, 0xFF=varies
         [3] animIndicator      (value-1) = number of extra anim state records
    +95  char[32] nadPath       Per-actor animation NAD path
    +127 uint8[13] tail         Fixed tail: float(30.0) at +5, uint32(17) at +9

  42 of 207 files contain actors (174 total actors found)

[Anim State Records: after actor base, count = animIndicator - 1]
  Type 2 (44 bytes):
    +0   uint32  type           Always 2
    +4   float   frameTime      Trigger frame (e.g. 5.67)
    +8   uint32  unknown        Reserved
    +12  char[32] name          State name (e.g. "anim_idle", "moving", "swing")

  Type 1 (48 bytes):
    +0   uint32  type           Always 1
    +4   float   frameTime      Trigger frame
    +8   uint32  unknown1       Reserved
    +12  uint32  unknown2       Reserved
    +16  char[32] name          Animation name (e.g. "siegeTank_SiegeFire")

  22 total anim states found across all files

[Global Events: after last actor to EOF]
  Sound Cues (string-scan heuristic):
    Patterns: "SF...", "env_...", "terran_...", "@music_...", "*.wav"
    25 files contain sound cue data

  Conversation Triggers:
    Pattern: "cM..." (e.g. "cM121_EscortSCV")
```

### Cross-reference

NCE files contain the same actor/camera/NAD data as their paired `.ncs` file. The NCS version has structured sound cue records with frame times; the NCE stores events with a heuristic layout. For reliable frame-time extraction, prefer the NCS.

---

## 21. NCS — Nihilistic Cutscene Script

**Extension:** `.ncs`
**Type:** Binary
**Version:** 1

Cutscene camera/shot scripts paired 1:1 with `.nce` event files. Contains actors, sound cues, and conversation triggers with structured records. 216 files in `Cutscenes/`. Validated: 216/216 files parse successfully.

### Header Variants

| variant1 | variant2 | variant3 | Count | Notes |
|----------|----------|----------|-------|-------|
| 1 | 4 | 0 | 118 | Most common |
| 1 | 0 | 0 | 56 | Camera-only (no actors) |
| 1 | 6 | 0 | 10 | |
| 0 | 0 | 0 | 8 | |
| 256 | 4 | 0 | 6 | |
| 1 | 7 | 0 | 4 | Includes embedded map names |
| *(other)* | | | 14 | Rare variants |

### File Layout

```
[Header: 24 bytes]
  +0   uint32   version        Always 1
  +4   uint16   variant1       Header sub-type field
  +6   uint16   variant2       Header sub-type field
  +8   uint16   variant3       Header sub-type field
  +10  uint16   shotCount      Number of camera shots (matches NCE shotCount)
  +12  uint16[3] reserved
  +18  uint16   eventCount     Number of scripted events (matches NCE eventCount)
  +20  uint16[2] reserved

[Camera Block: variable size, starts at byte 24]
  Base: ~94 bytes of camera config data (mostly zeros for NAD-driven cameras)
  Optional: 32-byte camera NAD path, validated by ".nad" suffix
  Camera block ends at first actor marker, or at end-of-file if no actors
  Sizes observed: 126 (15 files), 94, 178, 266, 270, 370, 406, 606, 686, 778, 2378

[Actor Records: variable, each preceded by 12-byte marker]
  Marker pattern: uint32(1), uint32(N), uint32(5) — N=1 (base only) or N=2 (has anim states)
  Validated by checking that byte at marker+12 is printable ASCII

  Per actor (140 bytes base):
    +0   uint32  marker1        Always 1
    +4   uint32  marker2        1 = base only, 2 = has anim state records
    +8   uint32  actorType      Always 5
    +12  char[32] modelName     Template model name
    +44  char[32] instanceName  Instance name
    +76  uint8[20] flags        Config flags (layout differs from NCE)
    +96  char[32] nadPath       Per-actor NAD path (position varies — see below)
    +128 uint8[12] tail         Fixed tail: uint32 unk, float(30.0), uint32 unk

  NAD Path Position (within actor record bytes 76–139):
    When uint32@+92 == 9999 (0x270F): NAD at +96, tail at +128
    When uint32@+92 == 0:             tail at +96, NAD at +108
    Parser scans for ".nad" suffix within bytes 76–140 for robustness

  31 of 216 files contain actors (95 total actors found)

[Anim State Records: after actor base, when marker2 == 2]
  +140 uint32  animStateCount  Total number of anim states (explicit count)
  Then animStateCount × 44-byte type-2 records:
    +0   uint32  type           Always 2
    +4   float   frameTime      Trigger frame
    +8   uint32  unknown        Reserved
    +12  char[32] name          State name

  9 total anim states across all files

[Sound Cue Block: type-3 header + structured records]
  Header (20 bytes):
    +0   uint32  type           Always 3
    +4   uint32  subType        Sub-type (typically 1)
    +8   uint32  reserved       0
    +12  uint32  cueCount       Number of sound cue records
    +16  uint32  reserved       0

  Per cue (76 bytes):
    +0   float   frameTime      Trigger frame (e.g. 1.1, 3.06, 8.26)
    +4   uint32  reserved       0
    +8   char[32] name          Sound cue name (e.g. "SF121ASCV42")
    +40  uint32  reserved       0
    +44  uint32  reserved       0
    +48  float   volume         Playback volume (typically 1.0)
    +52  float   fadeIn          Fade-in time (typically 0.1)
    +56  float   range          Audible range (typically 10.0)
    +60  uint8[16] padding      Zeros

[Conversation Block: type-2 header + name + frame time]
  Header (12 bytes):
    +0   uint32  type           Always 2
    +4   uint32  count          Number of conversations (typically 1)
    +8   uint32  reserved       0

  Per conversation (48 bytes):
    +0   char[32] name          Conversation ID (e.g. "cM121_EscortSCV")
    +32  uint8[12] config       Config/padding
    +44  float   frameTime      Trigger frame (e.g. 16.1)

  2 files contain conversation triggers
```

### NCS vs NCE Comparison

| Feature | NCE | NCS |
|---------|-----|-----|
| Header | Variable (version + mapName) | Fixed 24 bytes |
| Camera pos/target/FOV | Yes (111-byte block) | No (NAD-driven) |
| Actor marker | `u32(1),u32(1)` + model@+8 | `u32(1),u32(N),u32(5)` + model@+12 |
| Actor flags | 23 bytes | 20 bytes |
| Actor NAD offset | Fixed +95 | Variable (+96 or +108, scan for ".nad") |
| Anim state count | Implicit (animIndicator-1) | Explicit u32 at +140 |
| Sound cue frame times | Heuristic (often 0.0) | Structured (76-byte records) |
| Conversation frame times | Heuristic (often 0.0) | Structured (type-2 block) |

---

## 22. NMB — Nihilistic Model Bundle

**Extension:** `.nmb`
**Type:** Binary
**Version:** 1 or 2

Multi-model bundle format containing multiple LOD variants of models. 40 files in `3D/Models/`, each paired with an `.nnb` animation bundle.

### File Layout

```
[Header: 12 bytes]
  uint32  version      1 or 2
  uint32  modelCount   Number of models in bundle
  uint32  totalMem     Total memory estimate

[Per Entry]
  [Compact Header: 5 bytes]
    uint8   shaderCount
    uint8   boneCount
    uint8   meshGroupCount
    uint16  markerCount

  [Entry Data: variable]
    char[32]  name           Model name
    uint8     vertexSetCount
    uint32    flags
    float[3]  boundsMin
    float[3]  boundsMax

    [Vertex Sets]
      Per set: uint8 type + uint32 count + count×stride vertex data

    [Index Buffer]
      uint32 indexCount + indexCount × uint16

    [Shader Names: shaderCount × 32 bytes]

    [Bones: boneCount × 26 bytes]
      float[3] position (12B) + float[3] offset (12B) + uint8 parent + uint8 jointId

    [Blend Data]
      uint32 blendCount + blendCount × 24 bytes

    [Mesh Groups: meshGroupCount entries, variable]
      Additional per-group: uint8 originalGroup, numBlendShapes, groupLinkIndex, jointMapSize
      + jointMapSize bytes bone remap table

    [Markers: markerCount × 68 bytes]
    [Padding: 1 byte]
```

### Vertex Types (same as NOD)

| Type | Stride | Layout |
|------|--------|--------|
| 0 | 32B | pos + normal + uv |
| 1 | 36B | pos + normal + uv + boneIdx |
| 2 | 48B | pos + normal + uv + weights + boneIdx |
| 3 | 68B | stream 0 (type 0, 32B) + stream 1 (TBN, 36B) |
| 4 | 72B | stream 0 (type 1, 36B) + stream 1 (TBN, 36B) |
| 5 | 84B | stream 0 (type 2, 48B) + stream 1 (TBN, 36B) |

Types 3–5 use two sequential (non-interleaved) streams. See NOD vertex types for details.

---

## 23. NNB — Nihilistic Animation Bundle

**Extension:** `.nnb`
**Type:** Binary
**Version:** 1

Animation bundle paired 1:1 with `.nmb` model bundles. Contains references to `.nad` animation files. 40 files in `3D/Anims/` (7 are empty with count=0).

### File Layout

```
[Header: 12 bytes]
  uint32  version      Always 1
  uint32  animCount    Number of animation entries
  uint32  dataSize     Total data size

[Entries: variable]
  Per entry at 32-byte aligned boundaries:
    char[64]  name     .nad filename
    size_t    offset   Byte offset in bundle
```

---

## Coordinate System

All spatial data uses the **Direct3D left-handed Z-up** coordinate system:

| Axis | Direction |
|------|-----------|
| X | Right |
| Y | Forward (into screen) |
| Z | Up |

---

## Xbox Shader Types

Materials reference Xbox vertex/pixel shader programs identified by filename in `.nsa` properties. The `shaderType` NSA property maps to these original Xbox shader source files:

| shaderType value(s) | Xbox source files | Surface behavior |
|---|---|---|
| *(default)* | `default*.xvu` | Standard lit diffuse |
| `glossenvmap`, `envmap`, `detail`, `bumpenv` | `Envmap*.xvu + bumpreflect.ps` | Specular + environment reflection |
| `glow`, `scanline` | `selfglow.xpu/.ps` | Self-illuminating emissive |
| `fullbright`, `sky`, `add`, `halo`, `pageflip` | `fullbright*` | Unlit texture color |
| `ghostsuit`, `cloak` | `ghostsuit.xpu/.ps + cloak*.xvu` | Ghost suit Fresnel rim |
| `glass`, `etchedglass`, `fog`, `fogcolor` | `glass*` | Transparent glass + Fresnel |
| `bump` | `bump*.xvu + emBump.ps` | Normal/bump mapping |
| `metalgloss`, `metalenvmap` | `MetalEnvmap*.xvu` | Metallic environment reflection |
| `membrane`, `shield`, `psishieldhit` | `membrane*.xvu + shield.ps` | Energy membrane/shield |
| `lava` | `lava.xpu/.ps` | Scrolling lava + glow |
| `water`, `bumpwater` | `waterNM*.xvu + waterNM.ps` | Animated water |
| `alphadissolve`, `dissolveglow` | `alphadissolve.ps` | Alpha dissolve transition |

---

## Asset File Counts

| Category | Format(s) | Count | Location |
|----------|-----------|-------|----------|
| Textures | DDS, TGA | ~4500 | Various |
| Models | NOD | ~80 | `3D/Models/` |
| Animations | NAD | ~250 | `3D/Anims/` |
| Motion Sets | NMS | 1 | `3D/Motions/` |
| Levels | NIL | 8 | `Levels/` |
| Level FX | NLFX | 8 | `Levels/` |
| Placements | NPD | 8 | `Levels/` |
| Scene Data | NSD | 8 | `Levels/` |
| Materials | NSA | 8 | `Materials/` |
| Textures | XPR | ~12 | `Materials/`, `UI/` |
| Terrains | NRT/NUT | ~4 | `3D/Terrains/` |
| Collision | NOC | ~80 | `3D/Collision/` |
| Loc Text | NLT | 1 | `Misc/` |
| Loc Strings | NLS/NLU | ~30 | `Localization/` |
| Templates | NOT | 1 | `Templates/` |
| Effects | NFX | 1 | `Effects/` |
| UI Layout | NUI | 5 | `UI/` |
| Camera Keys | NAK | 3 | `3D/Anims/` |
| Cutscene Events | NCE | 207 | `Cutscenes/` |
| Cutscene Scripts | NCS | 216 | `Cutscenes/` |
| Model Bundles | NMB | 40 | `3D/Models/` |
| Anim Bundles | NNB | 40 | `3D/Anims/` |
