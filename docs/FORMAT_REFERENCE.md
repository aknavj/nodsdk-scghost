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

The primary level geometry format. Contains sector-based BSP/portal architecture,
per-vertex baked lighting, sector static lights for moving objects, and environment
map references. The verified NIL v35 layout does not contain a separate lightmap
texture atlas; older parser comments describing lightmap patches/atlases are not
evidence of one.

### File Layout

```
[Header]
  uint32  magic            "NIL\x10"
  uint32  version          35
  uint8[72] globalLight    Global light parameters
  float[3]  ambientColor   RGB ambient color
  uint32  numShaders       Material count
  char[]  shaderNames      numShaders × 32-byte names
  uint16  numEnvMaps       Environment map count
  char[]  envMapNames      numEnvMaps × 32-byte names
  uint32  numSectors       Sector count

[Sectors: numSectors entries]
  Per sector (variable size):
    uint32    flags
    float[3]  center       Sector center point
    uint8     numBoundaryPlanes
    float[4][] boundaryPlanes   Inward half-space planes (N × 16 bytes)
    uint8     numVertexPools   Observed: 0, 1 or 2
    [Per vertex pool]
      uint8   poolType       0=primary, 1=primary plus TBN stream
      uint32  numVerts       Native reads 4 bytes; uses low 16 bits
                            Original counts are at most 65535
      [Vertex Data: numVerts × 36 bytes]
        float[3] position   World-space position
        float[3] normal     Surface normal
        uint32   diffuse    BGRA packed vertex color (Xbox D3DCOLOR)
        float[2] uv         Texture coordinates
      [TBN Data: numVerts × 36 bytes, if poolType == 1]
        float[3] tangent
        float[3] binormal
        float[3] normal
      Original pool sequences: [], [0], [1] or [0, 1].
      These are not lightmap patches. Legacy parser names retain
      "hasGeo", count/padding and secondary-pool terminology.

    uint16    numGroups    Surface group count
    [Surface Groups: numGroups entries, 47 bytes each]
      uint32  shaderIdx        Material index
      uint32  startIdx         Index buffer start
      uint32  field8           Unknown; observed 0
      uint16  numStripIndices  Triangle strip index count
      uint16  numListIndices   Triangle list index count
      uint32  field16          Unknown; observed 0
      uint16  renderFlags      Not a vertex format
      uint8   vertexPool       Selects owning sector's vertex pool
      float[3] bboxMin         Group AABB minimum
      float[3] bboxMax         Group AABB maximum

    uint32    numIndices   Always present, even without vertex pools
    uint16[]  indices      Triangle strip/list indices

    [Things: variable]
      uint8   numThings
      [Thing: 68 bytes each]
        float[3]  position
        uint32    field1
        uint32    field2
        float[12] matrix    4×vec3 row matrix

    [Static Lights: variable]
      uint16  numStaticLights
      [tRenderLight: 72 bytes each]
        uint8[72] data      Native light record; legacy name "SurfThing"

    [EnvRefs: variable]
      uint16  numEnvRefs
      char[]  envRefNames   N × 32-byte names

    [Tail: 6 bytes]
      uint16  tailField1   Unknown; observed 0
      uint32  tailField2   Unknown; observed 1

[Post-Sector Data: verified no-terrain layout]
  uint16 terrainCount       0 in all eight original NIL fixtures
                            Nonzero terrain needs a separate decoder.
  uint16 physicsSetup       Observed 0; retained without inferred meaning
  float[6] physics_bounds    Min xyz, max xyz
  uint16 havokVersion       1

  Repeat:
    uint8 segmentType       0=end, 1=world rigid body
    If 1:
      uint16 vertexCount
      float[3][] vertices   vertexCount positions
      uint32 indexCount
      uint16[] indices     Triangle-list indices
      uint16 materialCount
      byte[][8] materials  Opaque material records
      float[4] moppInfo
      uint32 moppCodeSize
      byte[] moppCode      Havok MOPP acceleration bytecode
      uint16 ownerType     0=world, 1=sector, 2=terrain
      uint16 ownerIndex    Sector/terrain index when applicable

  For each sector:
    uint32 collisionPresent
    For each render surface group:
      uint32 mappingPresent
      If nonzero:
        uint32 collisionOffset
```

The original loader (`Ghost.exe` with matching `Ghost.map`) reads sectors,
terrain, pre-Havok setup, Havok body segments, then surface-group mappings:
`gameWorld_LoadWorld` at `0x00534410`, `renderTerrain_ReadFile` at `0x005e5670`,
`gameHavok_ReadPreHavokSetup` at `0x00518c40`, `gameHavok_ReadHavokFile` at
`0x00519380`, and `tWorld::ReadHavokSurfaceGroupOffsets` at `0x00533dc0`.
Mesh framing is read by `hkNSIMeshShape::ReadData` at `0x00518d10`;
MOPP framing by `simPhysics_ReadMoppCodeFromFile` at `0x004921b0`.
These are addresses in the supplied debug executable, not portable identifiers.

`NILCollision` independently decodes this framing with bounded reads and exact
EOF consumption. All eight original NIL files match. Earlier descriptions of
the post-sector bytes as a portal collision mesh followed by a lightmap atlas
were speculative and are superseded by the loader trace.

### Native Lighting Contract

The standalone [native lighting technical document](NATIVE_LIGHTING.md) covers
the full implementation, authoring contract, algorithms and validation.

World surfaces and moving characters use different lighting inputs:

- World vertex colors are packed D3DCOLOR at byte 24 of each 36-byte primary
  vertex. The original sector reader preserves these values in the vertex buffer.
  Without authored lights the fresh world compiler writes `0xffffffff` for
  every vertex: an unbaked white baseline, not generated illumination.
  Authored `nod_static_light` entities enable CPU direct-light vertex baking.
- The 72-byte sector records formerly called opaque `SurfThing` records are
  static `tRenderLight` entries. The native sector reader at `0x005f2b62` reads
  their uint16 count into sector offset `0x10`, then reads `count * 72` bytes into
  the array at sector offset `0xe4`. Original `tObject::GetStaticLights` at
  `0x00427690` selects from that same array. Baking vertex colors alone will not
  supply these lights to moving NPCs.
- Header bytes 8..79 are the global light record; bytes 80..91 are three ambient
  floats. Native `rlLight_SetAmbient(float3*)` at `0x006022d0` scales its incoming
  float channels by `0.16326530277729034` (constant at `0x007ab8a8`), not by
  `1/255`. The extended native oracle verifies header copying to `WORLD+0x108`,
  componentwise maximum with game-sector ambient at `+0xE8`, and rounded
  packed-ARGB output from the complete setter. For normalized authored ambient
  `a`, the compiler writes `clamp(a,0,1) * 255`. Native character shader setup
  at `0x005FCB50` subsequently normalizes scaled ambient RGBA by approximately
  `1/255` and uploads c116. CPU ambient uses `clamp(a,0,1) * 0.16326530277729034`.
  Do not invert that native attenuation: the initial extended-export mapping
  did so and produced ambient about 6.125 times too strong.

Verified static-light field offsets, without inferring the remaining fields:

| Byte | Type | Native use |
|------|------|------------|
| 0 | uint32 | Flags/metadata, retained without a complete bit interpretation |
| 4 | uint32 | Light type; original sampled sector lights use point type 0 |
| 8 | uint32 | Light-style index; 0 bypasses style lookup |
| 12 | float[3] | RGB channels in 0..255 color units |
| 24 | float | Render intensity; style-0 `renderLight_AddLight` sets it to input weight times field 28 |
| 28 | float | Selection/base intensity multiplier |
| 32 | float[3] | Direction used by the cone-light branch |
| 44 | float[3] | World position |
| 56 | float[2] | Retained without inferred meaning |
| 64 | float | Distance cutoff used by type 3; not a type-0 cutoff in object selection |
| 68 | float | Falloff coefficient |

For type 0, original `CalculateLightIntensity` at `0x004270e0` calculates the
object-selection weight as
`max(RGB)/255 * intensity / (1 + falloff*d + 1.2*falloff*d*d)`.
Here `d` uses the game's approximate square root, not an exact Euclidean square
root. This weight is the native light-selection result, not proof of the final
per-pixel shading equation or static shadow visibility. A separate native
`renderLight_AddLight`/`rlLight_AddLight` oracle captures the D3D point-light
setup: diffuse RGB is `RGB * renderIntensity / 255`, attenuation coefficients
are `(1, falloff, 1.2 * falloff)`, and range is the constant `9999`, not field 64.

`LightingNative` executes the original sector reader and object-light selection:
zero lights gather none; generated point records reach the selector unchanged;
color/intensity/falloff affect the real result; baked diffuse values reach native
vertex buffers unchanged; the ambient prefix uses its actual engine scale. Ambient
execution stops before graphics-state updates. No GPU render or complete game
frame is claimed.

The fresh compiler implements direct surface lighting with shared-edge
subdivision and brush occlusion, plus matching type-0 character-light records.
Both intensity fields are initialized from authored intensity. Falloff uses
the original `worldLighting_CalcLightFalloff` equation with cutoff `0.2`.
Explicit sun/moon uses a type-2 global record, normalized travel direction and
sector flag `0x20`; native setup produces D3D directional type 3. CPU sun rays
point against that direction. Explicit ambient uses the verified mapping above.
Both header inputs remain zero when disabled.
See the [authoring workflow](../src/tools/README.md#cpu-direct-light-baking).
Four-sample oversampling, optional brush shadows and angle-threshold normal
smoothing are compiler options. Indirect illumination, light styles, complete flag meanings, and per-sector
light assignment remain separate work. In particular, native character lights
can cross walls in the current single-sector output.

### Collision Validation Scope

Decoding these records does not establish runtime collision correctness.
The world compiler must not emit NIL until collision/spatial generation and
runtime loading are validated; a render-only NIL is not an acceptable substitute.

### Bounded MOPP Generation

`src/tools/common/mopp_compiler` builds a deterministic, balanced spatial tree
for one mesh subpart, up to 65535 vertices and 4096 triangles. Input triangle
indices remain the primitive keys; the second shape-key word is subpart zero.
Invalid, degenerate or unrepresentable geometry fails without changing output.

The generated subset uses byte-axis splits (`0x10`-`0x12`), two-offset
16-bit splits (`0x23`-`0x25`), axis bounds (`0x26`-`0x28`), and triangle terminals
(`0x30`-`0x51`). Multi-byte operands are big-endian within the bytecode.
`moppInfo.xyz` is the world offset; `moppInfo.w` converts to 16.16 fixed point.
Bounds include one quantization cell of slack. This is not a decoder for
arbitrary original MOPP programs.

Optional `MOPPNative` tests emulate the original debug routines:
`queryAabb` (`0x0063aae0`), `querySphere` (`0x0063a0d0`) and
`queryLongRay` (`0x0063bd80`). Accelerated ray results match the unaccelerated
mesh cast (`0x0062f520`) across seven acceleration fixtures and five CSG fixtures,
including 4096-triangle meshes, rotated/sloped surfaces, narrow gaps, negative
coordinates and different scales. The selected test room is an optional extra
fixture. Equal-distance shared-edge hits may select either native-valid triangle.
These are query checks, not full in-game world validation.

`hkNSIMeshShape::GetSoundType` (`0x004504e0`) also establishes part of each
8-byte material record: the first `uint32` is an exclusive triangle-key limit,
and the following `uint16` is the sound type. The final two bytes remain opaque;
original files contain nonzero values, so readers must preserve them.

### Collision Surface Lookups

`tSector::FindSurfaceGroupWithTri` (`0x0052ecb0`) scans groups in reverse order,
returning the last mapped group whose offset is at most the triangle key.
Offsets are inclusive triangle-key starts, not byte/index-buffer offsets.
The loader sets runtime group flag `0x02` when a mapping is absent, and clears
it when present. Keys below the first mapped start have no surface group.
Equal offsets are allowed; the later group wins.

Material lookup scans forward and uses the first record whose exclusive limit
exceeds the key. Missing records produce no sound override. `NILCollision`
exposes both lookups without changing its lossless raw material records.
Optional native tests exercise the same boundary and duplicate-offset cases.
Across all eight originals, present sectors each resolve to one sector-owned
collision body; offsets are ordered and at most its triangle count, and
material limits are ordered and within their body's triangle count.

`world_geometry` reconstructs bounded classic MAP half-spaces, then subtracts
other brushes' interiors from their polygons. Same-facing coplanar overlaps
retain the earliest source brush's material and classic texture fields;
opposing shared faces are removed. Split fragments retain their source brush.
Seam vertices are welded, precise-position T-junctions are inserted into edges,
and ear triangulation preserves collinear boundary vertices.

Each input solid is checked for closed manifold boundaries and positive volume.
The union mesh has balanced directed edges; edge/vertex-only contacts between
solids are retained rather than filled. Binary32 precision loss, excessive
fragment/work counts and more than 4096 triangles before/after CSG are rejected.
Geometry and MOPP output remain unchanged on failure.

The selected test room now has 32 exterior polygons, 32 collision vertices and
72 triangles. Exposed area is 2876 (down from 2964); solid volume remains 1302.
The triangle count stays unchanged because internal-face removal is accompanied
by seam-preserving triangulation. Native rays from the room hit the floor at
Z=0, ceiling at Z=11, X walls at -9/10 and Y walls at -7/7, while rays crossing
the four internal wall joins no longer hit. Exact cell-union tests independently
check volume and exposed area, alongside analytic sloped/rotated overlaps.
`world_render` prepares separate per-surface vertices with normals and classic
UVs while preserving collision triangle order. UV-axis selection uses the source
plane's binary32 cross product and double normalization, matching Radiant rather
than the CSG normal near dominant-axis ties. Cardinal rotations are exact; other
rotations, negative scales, shifts and image-size division follow Radiant's float
projection. CSG split vertices retain their source face's projection.

The read-only `nod_materials_describe` API returns canonical native shader names
(at most 31 bytes), diffuse references, native-script provenance and verified
editor DDS dimensions.
Native diffuse lookup retains shared/matching-level archive precedence.
Editor definitions are separately read from `Materials/shaderlist.txt`, so native
level overrides cannot alter authored UV scale. Editor images follow Radiant's
exact-path, `Materials/`-path and unambiguous-basename lookup, including format
priority. Missing unrelated shaderlist archives warn and are skipped, as in
Radiant; missing/ambiguous requested images fail. DDS headers, formats and top
mips are validated without generating preview files or accepting white fallbacks.
Non-DDS editor images, standalone NAS shaders and non-DDS-backed materials are
explicitly unsupported.

The selected room has 136 render vertices, 216 indices and two native materials:
`DefaultGrid16` (1024x1024 editor image) and `GE_tileConcreteA_wht` (256x256).
Surface seams account for the extra vertices; collision remains 32 vertices and
72 triangles. Projection, material and render builders preserve output on failure.
Generic tests cover orientation/tie rules, rotations, non-square images, negative
scales, CSG continuity, material aliases, bank overrides and malformed input.
`world_sector` encodes one primary-pool sector with triangle-list groups,
white vertex colors and no boundary planes/instances/environment references. Adjacent
same-material surfaces merge without changing triangle order; each group retains
its exact AABB and inclusive collision-key start. Shader indices retain material
order. Nonzero authoring flags and native scripted shaders are rejected until
their flags/TBN requirements are implemented.

The native reader and group draw routine verify the 36-byte vertices, 47-byte
disk groups, index ranges, flags and pool selection. All 619 original sectors
have opaque tail (0, 1); all original groups have zero field8/field16. The encoder
uses those values and observed standard-sector flags 5, without assigning
unverified meanings. Sector records do not include a world header or collision
body and cannot be published as NIL.
The selected room produces two groups and a 5462-byte sector record.

`SectorNative` executes the hash-pinned original reader and fallback draw
routine with mocked file, allocator and GPU boundaries. It checks small,
4096-triangle and selected-room records, exact vertex/index bytes, shader
pointers, bounds, draw inputs and truncation failures. It does not verify
shader execution, owner-pointer setup, visibility, complete level loading or
actual rendering.

`world_image` assembles a complete NIL v35 image in memory: generated header,
one sector, no terrain/environment maps, one sector-owned mesh/MOPP body, no
custom sound ranges and exact group collision mappings. It preserves the list
of excluded entities. Zero global-light/ambient records use the baseline also
observed in the original menu; authored lighting is not compiled or borrowed.
Image physics bounds must finitely, strictly enclose geometry.
Their float span must remain finite, and every collision vertex must lie within
the world bounds. The compiler regenerates MOPP and rejects a mismatched program.
The selected room produces an 8220-byte image. `MAPtoNIL --world-only` publishes
it through checked atomic replacement inside an existing workspace, outside the
asset root. CLI bounds default to the compiled world box padded by one MAP unit
per side, rounded outward if necessary; an explicit override remains available.
Non-world entities are excluded and counted, not included in automatic bounds.
The published file matches the image fixture and passes native loading/ray checks.

The native point-in-sector tests interpret each boundary record as `(nx,ny,nz,d)`
and accept a point when every `dot(n,p) >= d`. Legacy parsers called these
records portals. With zero planes, the single sector intentionally spans all
space; this does not remove the physics broadphase bounds.

`WorldNative` executes original sector/terrain/mesh/MOPP readers, physics-bound
loading, group mapping, world/sector postprocessing, default-shader group
initialization and point-to-sector selection. Native rays use the mesh/program
read from the emitted image and resolve their hits through native surface-group
lookup. File, allocator, GPU and progress facilities are mocked; minimal shader
objects use the real default-shader vtable but do not load textures.
Header framing and zero-light records are checked independently.
This component suite does not register a physics body.

`PhysicsNative` executes original `CreateHavokWorld` (`0x00493d10`),
`gameHavok_ReadHavokFile` (`0x00519380`), rigid-body construction, `hkWorld::addEntity`
and sector-property assignment. It checks the world backlink, fixed-island
membership/index, sector body pointer, native property key 1 and single-reference
ownership of the body, MOPP tree, mesh and code. Mesh/program bytes are read from
the actual registered body's shape, not a separately constructed query fixture.
The native world retains the explicit image bounds.

Original broadphase AABB queries return the generated body's collidable and
reject clear space. The original gameplay first-hit ray path checks distance,
position, inward normal, collidable, triangle key and native surface-group lookup.
This gameplay path uses sector-owned bodies directly; it is not itself proof of
broadphase traversal. Both paths are tested independently on small, 4096-triangle
and selected-room images, including all six interior boundaries and a short
interior miss. Fixture margins are test data, not a production-bounds policy.

Additional facility mocks cover checked size/tag allocation/free and profiling
timers. Allocations and frees are poisoned; unknown memory interfaces fail.
Native stack overflow/underflow routines execute unchanged. A dedicated x86 FS
segment backs the SEH chain while null memory remains unmapped.

`SolverNative` executes original sphere/inertia/body construction and
`hkWorld::stepDeltaTime` (`0x0064bd40`), including collision detection, integration
and `_hkSolveConstraints` (`0x006a2990`). The original scratchpad factory
(`0x0069d360`) supplies the solver workspace. Contact-listener and solver hooks
only observe execution; no contacts, impulses or resulting transforms are mocked.
Only the existing profiling facilities gain contact-timer backing.

Zero-gravity motion and gravity-driven free fall match semi-implicit integration
within 0.00002 units, with no contacts or constraint-solver calls. Contact cases
run 180 steps at 1/60 second: small and 4096-triangle floors plus all six selected
room boundaries. A radius-0.25 sphere rests 0.30 units from the source plane,
including the native mesh's 0.05 skin. Clearance never falls more than 0.005
below that distance. The final 60 steps hold position and speed within 0.001,
with at most 0.001 tangential drift. Actual contact events reference the
generated world body and probe body; the static world remains fixed.
The native contact constraint retains both entities. Probe creator references
remain owned by the test for its emulator lifetime.

These controlled sphere tests do not validate player/controller behavior,
arbitrary dynamics, NSD entities, shader/visibility rendering or complete game
loading. Those smoke tests are deferred, not gates on world-only publication.

`ShaderNative` loads the selected image with original shader registration,
factory dispatch, hash tables and `cShader::Load` (`0x005fdf80`). Original texture
resolution and D3DX file/DDS parsing execute through native CPU mip generation.
Read-only file, allocation, display-mode and GPU-resource facilities are mocked;
the GPU buffers are CPU-backed, not rendered. Missing-texture fallback requests
fail explicitly. Shader/texture names, loaded state, dimensions, format, references
and group shader pointers are checked.

`DefaultGrid16` loads as 1024x1024 DXT1 with 11 mip levels;
`GE_tileConcreteA_wht` loads as 256x256 DXT3 with 9. Base uploads match the original
DDS payloads exactly; all native-generated levels receive writes. Cache hits
retain the original shader without new file reads, uploads or allocations.
Checked memory domains and resource references reject invalid frees/levels.

Original visible-sector list building and recursion retain the single zero-portal
sector, clip rectangle and generation, including rollover. Original box-frustum
construction/AABB tests select both groups, reject distant/touching bounds and
isolate each material run. These are CPU visibility prerequisites, not camera
projection, portal traversal, sector rendering or GPU shader execution.

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

## 10. NPD — Navigation Graph

**Extension:** `.npd`
**Type:** Binary

AI navigation waypoint graph for a level, with **world-space XYZ positions** and
directed links. The original loader copies position floats unchanged, then calls
the world's sector lookup with those same positions. Earlier claims here about
a separate approximately 4x XY scale and compressed/floor-index Z were incorrect.

NPD is not entity placement data; NSD supplies entity placements. A waypoint ID
is a unique navigation identifier, not an NSD entity reference. The original
loader and path search are exercised by `PathNative` using freshly compiled NPD.

### Header (8 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 2 | version | Fresh writer uses 2; original runtime accepts 1 or 2 |
| 0x02 | 2 | nodeCount | Number of valid waypoint records |
| 0x04 | 2 | linkCount | Number of directed link records |
| 0x06 | 2 | nextUniqueId | Next editor waypoint ID |

### Waypoint Records (nodeCount × 26 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 2 | waypointId | Unique waypoint identifier (u16) |
| 0x02 | 12 | position | World-space XYZ (3 float32 values) |
| 0x0E | 2 | flags | Node flags; fresh ordinary nodes use 0 |
| 0x10 | 1 | component | Connectivity group; native search compares groups first |
| 0x11 | 2 | associationA | Associated-link index, FFFF when absent |
| 0x13 | 2 | associationB | Associated-link index, FFFF when absent |
| 0x15 | 2 | sector | Sector index; original loader recomputes it |
| 0x17 | 2 | firstOutgoing | First outgoing link's array index |
| 0x19 | 1 | outgoingCount | Number of outgoing links, at most 255 |

### Directed Links (linkCount × 6 bytes)

Links follow the waypoint records and must be grouped by source node to match
each node's outgoing range. Each entry is 6 bytes:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 2 | fromNode | Source node index (0-based record index) |
| 0x02 | 2 | toNode | Destination node index (0-based record index) |
| 0x04 | 2 | flags | Link flags, not a numeric cost; fresh ordinary links use 0 |

Endpoints are **0-based record indices**, not waypoint IDs. The loader derives
normalized direction vectors from endpoint positions. Reverse travel requires
a separately authored reverse link.

`MAPtoNPD` resolves authored waypoint IDs to record indices, groups outgoing
links, and assigns weakly connected component IDs 0..254. It supports one sector
(0), normal flags, and absent associations. Special traversal/association
semantics are not yet reconstructed. Fresh IDs are 0..65534; at most 65535 nodes,
65535 links, 255 outgoing links per node, and 255 components are representable
by this authoring profile. Zero, underflowing and overflowing squared link
lengths are rejected before native normalization.

The legacy `NPDParser` keeps its version-2, byte-preserving API: `color` and
`extra` are raw packed views of offsets 0x10..0x19, not visualization data, and
`LinkRecord::cost` is the old name for link flags. Correct decoded accessors are
provided without changing legacy serialization. Its permissive parsing is not
a substitute for the new graph compiler's validation. Existing source-bound
`nod_id` imports and raw edit keys are not fresh graph authoring entities.
Their legacy `nod_space=navigation` tag is retained for source-workflow
compatibility; it does not imply a different physical coordinate system.

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

### Prefix and first object (verified Ghost scenes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 4 | magic | `NSD\x10` (0x1044534E) |
| 0x04 | 4 | version | Scene version; supplied scenes use 11 |
| 0x08 | 64 | metadata | Two 32-byte fields; semantics not fully reconstructed |
| 0x48 | 4 | flags | Scene flags |
| 0x4C | 4 | numTemplateFiles | Count of following 32-byte NOT archive names |
| 0x50 + 32*N | 4 | objectCount | Declared object count after the archive-name list |
| 0x54 + 32*N | 4 | modifierCount | First object's modifier count, **not a sector count** |
| 0x58 + 32*N | 4 | modifierID | First modifier ID, normally 0 (`basedon`) |
| 0x5C + 32*N | 32 | templateName | Payload of the first `basedon` modifier |

`N` is `numTemplateFiles`. All eight supplied scenes have no extra archive
names, so the commonly observed first template name starts at byte 92.
The verified loader stores archive handles at scene offsets `0x160` through
`0x17C`: eight slots before the object-pointer array at `0x180` and world pointer
at `0x184`. `MAPtoNSD` therefore caps extra archives at eight. The legacy SDK
scanner accepts up to nine nonempty, NUL-terminated ASCII names (32 bytes each), validates
the entire list, reads the shifted object count, and starts placement scanning
after that list. Names are exposed in source order. Archive names are not
placement/type strings or sky-model references.
The parser still uses a tag-validated placement scan rather than a complete
sequential decoder for frames, parameters, inline models, sectors, camera data,
lights, sounds, and subsequent scene sections.

### Minimal compiled point-scene profile

`MAPtoNSD` writes version 11, empty metadata, zero flags, a verified ordered
archive list and an object count. Each supported point placement has a counted
36-byte modifier array, followed by this complete 20-byte empty tail:

| Bytes | Field | Compiled value |
|-------|-------|----------------|
| 4 | motion-frame count | 0 |
| 4 | script-parameter count | 0 |
| 4 | inline-model-present flag | 0 |
| 4 | sector-link count | 0 |
| 2 | camera-region-present flag | 0 |
| 2 | prelighting count | 0 |

The trailer after all objects is 192 bytes: a zero sound count, three zero
auxiliary world-array counts, two empty 32-byte fields, 48 zero bytes of numeric
environment fields, an empty 32-byte sky-model reference and an empty 32-byte
sky-animation reference. Scene flags remain zero, so the optional `0x08000000`
section is absent. This is a bounded empty-scene profile, not a general
sequential decoder for arbitrary NSD content.

Original native scene/file readers consume this entire framing in local tests.
Object construction and lifecycle/postprocessing are substituted; the test
compares the exact modifier bytes handed to the native object-factory boundary.
It does not prove actual Nova/player/controller creation or resource loading.
Supported placement types are 0, 2, 7 and 8. Type 8 is `tContainer`, not a
brush-region record. The native `gameThing_New` dispatch at `0x52FE10` allocates
`0x3C0` bytes and invokes `tContainer::tContainer` at `0x518380`, whose completed
object has vtable `0x78E538` and native type 8 at offset `0x10`. A separate test
executes that dispatch and its original constructor chain, substituting only
heap allocation. The mixed-scene test checks a type-8 template reference and
local override through the original scene reader's object-factory boundary.
Container resource loading, inventory and gameplay initialization remain
outside these tests.

### Entity Block Layout

Objects contain a counted array of **36-byte runtime modifiers**, shared with
NOT. The commonly observed first template-name payload is at offset 92; it is
not the beginning of an object record. Relative to that payload at `T`:

```
[Modifier Count: uint32 at T-8]
[First modifier ID: uint32 at T-4, normally 0 = basedon]
[Template Name: 32 bytes at T]
  Null-terminated ASCII string, zero-padded to 32 bytes.

[Property Block 0 (rotation): 36 bytes at T+32]
  uint32  tag1        = 1 (rotation tag)
  float[3] rotation   Euler angles (pitch, yaw, roll in degrees)
  byte[20] filler     Leftover name bytes / padding

[Property Block 1 (position): 36 bytes at T+68]
  uint32  tag2        = 2 (position tag)
  float[3] position   World-space position (D3D Z-up coordinates)
  byte[20] filler     Leftover name bytes / padding

[Property Block 2: 36 bytes at T+104 in common records]
  uint32  tag         Often 3 (sector) or 15 (script)
  byte[32] payload    Interpret using the modifier ID, not the slot number

[Property Block 3+ (optional): 36 bytes at T+140]
  uint32  tag         E.g. 4 (model), 46 (CASTID), 161 (EFFECT1)
  byte[32] payload    Fixed width even for scalar/vector values
  ...                 Additional property blocks follow in 36-byte steps
```

### Tag Values

| Tag | Meaning | Data |
|-----|---------|------|
| 0 | basedon | Template reference, resolved case-insensitively |
| 1 | Rotation | 3 floats: pitch, yaw, roll (degrees) |
| 2 | Position | 3 floats: X, Y, Z (D3D world-space) |
| 3 | sector | Sector ID (u32); remaining payload bytes are not additional properties |
| 4 | Model name | Null-terminated string (e.g., "door.nod") |
| 15 | script | Script class/reference; often used by region records |
| 20 | type | Runtime thing-type enum, distinct from the template name |
| 46 (0x2E) | CASTID | String; **not** the script modifier |
| 47 | DESCRIPTIONID | String; **not** root/template flags |
| 143 | MODELSCALE | Float32 |
| 161 (0xA1) | EFFECT1 | String; **not** the script modifier |
| 175 | MODELOFFSET | Three float32 components |

The field historically called `type_name` identifies a template, not the runtime
thing class. Expand its modifiers to obtain `type`, `model`, scripts, flags,
and other properties. Characters, decorations, and region-like templates should
not be assigned model aliases or skipped solely from a hard-coded name list.

### Inline Region Geometry

For counted region records, the 32-byte type name is the payload of modifier
tag 0. A modifier count precedes that tag, eight bytes before the type name.
The count includes the type modifier, and each modifier occupies 36 bytes.
After the modifiers come:

1. A `uint32` motion-frame count and that many 28-byte frames.
2. A `uint32` script-parameter count and typed parameters. Types 1-5 and 8-11
   have a four-byte payload; type 6 has none; type 7 stores a `uint32` byte
   length followed by that many string bytes. Each parameter starts with a
   four-byte type.
3. A `uint32` inline-model-present flag. A nonzero flag is followed by a
   standard 92-byte NOD header and its shader/bone/vertex/index/group streams.
   The inline model's trailing marker representation is not needed for bounds.

The region parser computes a local AABB from vertices referenced by valid
LOD-0 triangles, handling per-set bases, split TBN streams, triangle lists,
degenerate connectors, and strip restarts. Header AABBs and unreferenced
vertices do not define the computed result. Native MatrixPYR rotation and the
entity position transform this box into a world-space bounding envelope.
Meshes and trailing region data remain byte-preserved, not rewritten.

This layout was verified against the native scene/frame/parameter/inline-model
readers and all 286 regions in the seven complete supplied Xbox level triplets.
Unknown layouts/types are explicitly marked unavailable rather than searched
heuristically for plausible floating-point bounds.

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

Entity template database containing 1,224 templates in the supplied Global.not.
Inheritance is an ordered expansion of modifier references, not necessarily a
single-parent tree.

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
    uint32  modifierCount
    [modifierCount × 36 bytes]
      uint32   modifierID
      byte[32] payload
    uint32  editorDataLength
    byte[editorDataLength] editorData
```

The first modifier is often `basedon` (ID 0), but some grouping templates begin
with `DESCRIPTIONID` (ID 47). There is **no flags=47 header field**. ID 1 is the
orientation vector with a full 32-byte payload, **not a five-byte boolean**.
Comments/empty editor strings are part of the separate editor-data block,
not modifiers. Unknown editor bytes and archive tail bytes must be retained.
The supplied final `scourge_track` record has 36 bytes beyond the loader-consumed
record; the tools preserve and diagnose them rather than invent a property.

### Expansion and model lookup

- Startup opens `global`, then scene loading opens the NSD's archive names in
  their declared order, relative to `Misc/Templates`. Missing filename
  extensions receive `.not`; archive lookup and names are case-insensitive.
  The SDK reads extracted files, including nested relative paths, and rejects
  absolute/traversing paths.
- All open archives share one template-name registry. The hashtable refuses a
  duplicate key, so **the first-loaded definition wins**, including Global over
  scene archives. This is separate from last-value-wins modifier expansion below.
  Reopening an archive does not register it twice. SDK diagnostics identify
  shadowed templates and the winning archive.
- References may cross archive boundaries, including references to definitions
  in a later archive. All requested files are indexed before expansion.
  The SDK publishes a replacement context only after every requested file is
  available and valid. Unlike the engine's warning-and-continue load path, tools
  reject incomplete archive contexts rather than preview with partial data.
- Names are matched case-insensitively.
- Process modifiers in source order. For ID 0, expand the named template at that
  position. Other IDs replace an existing expanded entry of the same ID, or append
  a new entry. A later reference can therefore override an earlier local value.
- Missing templates, cycles, and expansion beyond 128 modifiers fail explicitly;
  no partial effective-property list is returned by the SDK.
- The same resolver accepts a complete NSD placement array. Local values retain
  placement provenance; referenced values retain their supplying NOT template.
  A local model does not bypass earlier missing/cyclic references, and a later
  reference can replace that local model. Preview model selection and the NSD
  editor's effective-property view use this shared expansion for counted arrays.
- Runtime creation reads ID 20 (`type`), then dispatches expanded modifiers to
  thing/actor/object/script/etc. handlers. ID 4 supplies the object's model.
- An explicit empty model override remains empty; it is not a request to guess
  a filename from the entity name.

### Verified static model transforms

Expanded modifiers execute in their resolved order. ID 4 creates the model
instance with uniform scale one. ID 143 (`MODELSCALE`) sets uniform scale only
after an instance exists; earlier values are ignored. Zero and negative scales
are not rejected by the engine setter. ID 174 (`MODEL_LODSCALE`) changes LOD
selection, not geometry.

ID 175 (`MODELOFFSET`) is added directly to the model-to-world translation when
renderflags (ID 12) have bit `0x2000`; it is not rotated or scaled:
`world = origin + enabled_offset + rotation * (scale * model_vertex)`.
The object constructor does not explicitly initialize this vector. Tools reject
an enabled offset with no explicit value rather than inventing one. Inactive
offset payloads do not affect static previews, and names ending in `_offset`
have no special transform meaning.

Evidence: `0x52ec90` (object modifiers), `0x6884e1` (new instance, scale one),
`0x52f7d0` (uniform scale setter),
`0x43d870`/`0x43d8d0` (offset gate/accessor), `0x546cc0` (renderflags),
`0x43d080` (world-space offset addition), and `0x687172` (model prescale).
The 3,379 observed placements include two scales of 0.75 and 42 active nonzero
offsets; every active offset has ID 175. Static previews do not reconstruct
animation, skeleton updates, or every gameplay handler.

### Verified metadata and evidence

The engine memory export contains 177 records at `0xA05250`, each
`name[32] + uint32 handler + uint32 nativeType` (40 bytes). Dispatch words start
at `0xA05270`. The SDK's `NODProperties` catalog matches every record.
Type codes 0/2/3/4 correspond to string/float32/integer bits/vector3.
Integer signedness, flag bit names, enum names, handler-specific semantics, and
cross-file validity are not inferred from that storage code alone.
ID 68 (`PARTICLENAME`) has unusual code 68 in this export and remains raw.

Loader/expansion evidence in the supplied `scghost-rev` export:
`0x68efb0` (NOT archive + editor data), `0x68f680` (count × 36-byte modifiers),
`0x543f60` (ordered replacement), `0x68f520` (case-insensitive lookup),
`0x53f2e0` (NSD modifiers and following sections), `0x52ec90` (object model),
`0x68edb0` (Global opened at startup), `0x53dd10` (scene archive list),
`0x6eab30` (duplicate hash keys are not replaced), and `0x6eb540`
(case-insensitive key comparison). The archive registry has 64 loaded-file slots;
the scene structure's nine handles bound a single scene's declared list.
The reconstructed wrappers contain assumptions; the raw decompile and real
Ghost files are used to cross-check them.

All 3,379 observed placements across eight NSDs have a verified counted array,
one `basedon`, no repeated modifier IDs, and IDs within 0–176. This does not
establish a complete NSD serializer or full gameplay schema.
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
  uint32  totalMem     Native object-arena allocation (not file/GPU-buffer size)

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
      4-byte packed marker identity + 4x4 float matrix
    [Version 2 only]
      uint8 hasColors (0 or 1)
      If 1: packed colour bytes, 4 bytes per vertex across all sets
```

### Verified NOD/NOC v10 compilation

`NODtoNMB` emits version 2 and model flags `0x00000a00` for the verified v10
input layout. NOC collision models use the same model-record layout and retain
their `.noc` member identities. The writer does not substitute visible NOD
meshes for collision inputs.

The target arena allocation is summed per member:
`328 + 4*shaderCount + 32*boneCount + 108*meshGroupCount + 80*markerCount`.
Vertex/index/TBN/colour buffers and blend buffers are allocated separately by
the runtime. These are target-runtime sizes, independent of host pointer width.
The formula matches 39 of the 40 local original bundle headers; the shipped
`2_3_1a_Escape_Mini.nmb` header is 480 bytes lower. New outputs calculate the
allocation rather than copying that exceptional header.

The 56-byte source mesh record maps to compact NMB records as follows:
shader index `[0:4]`, vertex-set byte `[54]`, four LOD records `[4:28]`,
original-group/blend-count/link bytes `[30:33]`, palette size `[53]`, then
that many palette bytes from `[33:53]`. Source vertex advance `[28:30]`
must equal LOD-0 vertex count because the bundle derives per-set bases from it.
Blend-shape counts and unknown reserved extensions are rejected by the current
compiler, rather than silently discarded.

NOD markers contain a packed 4-byte identity and twelve floats (four 3-element
rows). Compilation appends a fourth component `0, 0, 0, 1` to those rows.
Bone positions/offsets/parent/joint map to the 26-byte native record; source
orientation matrices and padding are not fields of bundled bones. Optional
source colour extensions are retained as the version-2 colour stream.

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
