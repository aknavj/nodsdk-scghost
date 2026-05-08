# Format Mapping & Dependencies

How StarCraft: Ghost asset formats reference each other, the naming conventions that bind them, and the data relationships between formats.

---

## Cross-Format Dependency Graph

```
                            ┌─────────────────────────────────────────────────────────┐
                            │                   TEXTURE FORMATS                       │
                            │                                                         │
                            │    ┌───────┐     ┌───────┐     ┌───────┐               │
                            │    │  DDS  │     │  TGA  │     │  XPR  │               │
                            │    │.dds   │     │.tga   │     │.xpr   │               │
                            │    └───┬───┘     └───┬───┘     └───┬───┘               │
                            │        │             │             │                    │
                            │        │  raw pixel  │  raw pixel  │ packed archive     │
                            │        │  data       │  data       │ of DDS-like imgs   │
                            │        └─────────────┴─────────────┘                    │
                            └──────────────────────┬──────────────────────────────────┘
                                                   │
                                                   │ referenced by name
                                                   │
                       ┌───────────────────────────┼──────────────────────────┐
                       │                MATERIAL FORMAT                      │
                       │                           │                         │
                       │                    ┌──────┴──────┐                  │
                       │                    │     NSA     │                  │
                       │                    │  .nsa .nas  │                  │
                       │                    │             │                  │
                       │                    │ texture   ──┼──→ DDS name     │
                       │                    │ glow      ──┼──→ DDS name     │
                       │                    │ detail    ──┼──→ DDS name     │
                       │                    │ envmap    ──┼──→ DDS name     │
                       │                    │ shaderType──┼──→ Xbox shader  │
                       │                    └──────┬──────┘                  │
                       └───────────────────────────┼──────────────────────────┘
                                                   │
                                                   │ material names referenced
                                                   │ by geometry formats
                                                   │
          ┌────────────────────────────────────────┼────────────────────────────────┐
          │                                        │                                │
          │                               GEOMETRY FORMATS                          │
          │                                        │                                │
          │     ┌──────────────────────────────────┼──────────────────────┐         │
          │     │              ┌────────────────────┤                     │         │
          │     ▼              ▼                    ▼                     │         │
          │  ┌──────┐    ┌──────────┐         ┌──────────┐               │         │
          │  │ NOD  │    │   NMB    │         │   NIL    │               │         │
          │  │ .nod │    │   .nmb   │         │   .nil   │               │         │
          │  │      │    │          │         │          │               │         │
          │  │model │    │ model    │         │ level    │               │         │
          │  │mesh  │    │ bundle   │         │ sectors  │               │         │
          │  │bones │    │ (N×NOD)  │         │ portals  │               │         │
          │  │mats ─┼─┐  │ mats  ──┼─┐       │ lightmap │               │         │
          │  └──┬───┘ │  └────┬─────┘ │       │ mats  ──┼─── NSA        │         │
          │     │     │       │       │       └──┬──┬───┘               │         │
          │     │     └───────┼───────┴──── NSA  │  │                   │         │
          │     │             │                  │  │                   │         │
          └─────┼─────────────┼──────────────────┼──┼───────────────────┘         │
                │             │                  │  │                              │
   ┌────────────┼─────────────┼──────────────────┼──┼──────────────────────────────┘
   │            │             │                  │  │
   │            │    ANIMATION FORMATS           │  │        SCENE FORMATS
   │            │             │                  │  │
   │            │    ┌────────┴───┐              │  │    ┌──────────────────────┐
   │            │    │    NNB     │              │  │    │                      │
   │            │    │    .nnb    │              │  ├───▶│  NSD  (.nsd)         │
   │            │    │ anim refs  │              │  │    │  entity instances    │
   │            │    └─────┬──────┘              │  │    │  positions/rotations │
   │            │          │                     │  │    └──────┬───────────────┘
   │            │          ▼                     │  │           │
   │            │    ┌──────────┐                │  │           │ entity type name
   │            │    │   NAD    │                │  │           │ → .nod model path
   │            │    │   .nad   │                │  │           ▼
   │  NOD bone ─┼───▶│ tracks   │                │  │    ┌──────────────┐
   │  indices   │    │ keyframe │                │  │    │ NOD (entity) │──── NSA ──── DDS
   │  drive     │    │ cubic    │                │  │    └──────────────┘
   │  tracks    │    └──────────┘                │  │
   │            │                                │  │    ┌──────────────────────┐
   │            │    ┌──────────┐                │  ├───▶│  NPD  (.npd)         │
   │            │    │   NMS    │                │  │    │  placement records   │
   │            │    │   .nms   │                │  │    └──────────────────────┘
   │            │    │ motion   │── NAD paths    │  │
   │            │    │ name map │                │  │    ┌──────────────────────┐
   │            │    └──────────┘                │  └───▶│  NLFX (.nlfx)        │
   │            │                                │       │  effect name list    │
   │            │    ┌──────────┐                │       └──────────┬───────────┘
   │            │    │   NAK    │                │                  │
   │            │    │   .nak   │                │                  ▼
   │            │    │ camera   │                │       ┌──────────────────────┐
   │            │    │ keyframe │                │       │  NFX  (.nfx)         │
   │            │    └──────────┘                │       │  effect controllers  │
   │            │                                │       │  particle systems    │
   │            │  COLLISION FORMAT              │       └──────────────────────┘
   │            │    ┌──────────┐                │
   │            │    │   NOC    │                │
   │            │    │   .noc   │                │
   │            │    │ col mesh │                │
   │            │    └──────────┘                │
   │            │                                │
   └────────────┴────────────────────────────────┘

     TEMPLATE FORMAT                      CUTSCENE FORMATS

     ┌──────────┐                         ┌──────────┐
     │   NOT    │                         │   NCE    │
     │   .not   │                         │   .nce   │
     │ entity   │                         │ cutscene │
     │ template │──── defines schemas     │ events   │
     │ hierarch │     for NSD entity      └────┬─────┘
     └──────────┘     types                    │ paired 1:1
                                               ▼
     LOCALIZATION                         ┌──────────┐
                                          │   NCS    │
     ┌──────────┐    ┌──────────┐         │   .ncs   │
     │   NLT    │    │ NLS/NLU  │         │ cutscene │
     │   .nlt   │    │.nls .nlu │         │ camera   │
     │ raw text │    │ key=val  │         │ shots    │
     └──────────┘    │ strings  │         └──────────┘
                     └──────────┘

     UI FORMAT                            TERRAIN FORMAT

     ┌──────────┐                         ┌──────────┐
     │   NUI    │                         │ NRT/NUT  │
     │   .nui   │                         │.nrt .nut │
     │ screen   │──── DDS (icon refs)     │ terrain  │
     │ layouts  │                         │ patches  │
     └──────────┘                         └──────────┘
```

---

## Naming Conventions That Bind Formats

### Same-Stem Pairing (Level Files)

Level assets share a common filename stem. Given a level `1_2_1a_Miners_Bunker`:

```
Levels/
  1_2_1a_Miners_Bunker.nil      geometry + portals + lightmaps
  1_2_1a_Miners_Bunker.nsd      entity instances (types, positions, rotations)
  1_2_1a_Miners_Bunker.npd      entity placement records
  1_2_1a_Miners_Bunker.nlfx     effect name references

Materials/
  1_2_1a_Miners_Bunker.nsa      material definitions for this level
  1_2_1a_Miners_Bunker.xpr      packed texture archive for this level
```

All six files derive from the same stem. The `.nil` file is the entry point — companion files are found by extension substitution.

### Same-Stem Pairing (Model Files)

Model bundles and animation bundles share stems:

```
3D/Models/
  <name>.nod      standalone model
  <name>.nmb      model bundle (multiple LODs)

3D/Collision/
  <name>.noc      collision mesh

3D/Anims/
  <name>.nnb      animation bundle (paired with .nmb)
  <name>.nad      standalone animation clip
```

### Same-Stem Pairing (Cutscene Files)

Cutscene event/script pairs share stems:

```
Cutscenes/
  <name>.nce      cutscene events
  <name>.ncs      cutscene camera/shot script
```

### NSA Material Name → Texture Path

NSA materials reference textures by name (no path, no extension). The name must be resolved against the directory tree:

```
Materials/Common/          shared textures
Materials/Metal/           metal surface textures
Materials/Stone/           stone surface textures
Materials/Wood/            wood surface textures
Materials/Organic/         organic surface textures
Materials/Glass/           glass/transparent textures
Materials/Effects/         VFX textures
Materials/Terrain/         terrain blend textures
Materials/Colors/          solid color textures
Materials/Lights/          light cookie textures
Materials/Shaders/         shader-specific textures
Materials/                 level-specific textures
3D/Materials/              model-specific textures
```

Resolution is case-insensitive. The `.dds` extension is implied. NSA texture name normalization strips `.dds` suffixes and trailing whitespace-separated tokens (e.g. `"rock_wall normals"` → `"rock_wall"`).

### NIL/NOD Shader Name → NSA Material

NIL surface groups and NOD mesh groups store material names as fixed-width strings (32 or 33 bytes, null-padded). These names are keys into the NSA material database, matched case-insensitively.

### NSD Entity Type → NOD Model

NSD entity records contain an entity type name string. For model-type entities, a secondary scan at offset T+140 locates a `.nod` model filename in 36-byte blocks. The model path is constructed as `3D/Models/<name>.nod` and resolved case-insensitively or by fuzzy matching (stripping underscores).

### NMS Motion Name → NAD Path

NMS maps gameplay state names to `.nad` animation file paths via tab-delimited text entries.

### NLFX Effect Name → NFX Controller

NLFX lists effect names (one per line) that reference controller blocks defined in the global `game.nfx` file.

---

## Level Format Relationships

Loading a level requires resolving a chain of cross-format references:

```
  ┌─────────────────────────────────┐
  │  NIL (.nil) — level geometry    │
  │                                 │
  │  Contains:                      │
  │  • shader name table            │──────┐
  │  • sector vertices + indices    │      │
  │  • surface groups with          │      │
  │    shaderIdx into name table    │      │
  └─────────────────────────────────┘      │
                                           │ shader names
                                           ▼
  ┌─────────────────────────────────┐      ┌─────────────────────────────────┐
  │  NSA (.nsa) — material defs     │◀─────│  name lookup (case-insensitive) │
  │                                 │      └─────────────────────────────────┘
  │  Per material entry:            │
  │  • texture    → DDS filename    │──────┐
  │  • glow       → DDS filename    │──────┤
  │  • detail     → DDS filename    │──────┤
  │  • envmap     → DDS filename    │──────┤
  │  • texture2   → DDS filename    │──────┤
  │  • shaderType → Xbox shader ID  │      │
  │  • intensity  → float scalar    │      │
  │  • detailscale→ float scalar    │      │
  │  • pageflip   → int frame count │      │
  └─────────────────────────────────┘      │
                                           │ texture names
                                           ▼
  ┌─────────────────────────────────┐      ┌─────────────────────────────────┐
  │  DDS/TGA/XPR — texture data     │◀─────│  search 13 directories          │
  │                                 │      │  case-insensitive matching      │
  │  DDS: DXT1, DXT5, A8R8G8B8     │      └─────────────────────────────────┘
  │  TGA: BGR/BGRA, RLE or raw     │
  │  XPR: packed archive, multiple  │
  │       named textures per file   │
  └─────────────────────────────────┘

  ┌─────────────────────────────────┐
  │  NSD (.nsd) — scene entities    │
  │                                 │
  │  Per entity:                    │
  │  • type name (ASCII string)     │
  │  • position  (3 floats, D3D)    │
  │  • rotation  (3 floats, euler)  │
  │  • model ref (for model types)  │──────┐
  │                                 │      │
  │  Entity types:                  │      │
  │  • ghost — player spawn         │      │
  │  • trigger_region               │      │
  │  • camera_region                │      │
  │  • alarm_region                 │      │
  │  • script_object                │      │
  │  • (model entities)             │      │
  └─────────────────────────────────┘      │
                                           │ model name
                                           ▼
  ┌─────────────────────────────────┐      ┌─────────────────────────────────┐
  │  NOD (.nod) — entity model      │◀─────│  3D/Models/<name>.nod           │
  │                                 │      │  case-insensitive / fuzzy match │
  │  Same format as standalone NOD  │      └─────────────────────────────────┘
  │  Mesh groups reference NSA mats │──── NSA ──── DDS (same pipeline)
  └─────────────────────────────────┘

  ┌─────────────────────────────────┐
  │  NPD (.npd) — placements        │
  │                                 │
  │  Per record (26 bytes):         │
  │  • entityId (uint16)            │
  │  • position (3 floats, D3D)     │
  │  • flags    (uint16)            │
  │  • color    (uint32)            │
  └─────────────────────────────────┘

  ┌─────────────────────────────────┐
  │  NLFX (.nlfx) — effect list     │
  │                                 │
  │  One effect name per line       │──────┐
  └─────────────────────────────────┘      │
                                           │ name lookup
                                           ▼
  ┌─────────────────────────────────┐
  │  NFX (.nfx) — effect database   │
  │                                 │
  │  controller blocks matching     │
  │  names from NLFX                │
  └─────────────────────────────────┘
```

---

## Model Format Relationships

### NOD Internal References

```
  NOD (.nod) file contents:
  ┌──────────────────────────────────────────────────┐
  │                                                  │
  │  Header                                          │
  │  • shaderCount, boneCount, vertexSetCount,       │
  │    meshGroupCount, indexCount                     │
  │  • boundsMin, boundsMax (AABB)                   │
  │                                                  │
  │  Shader Names ─────────────────────────────────┐ │
  │  • shaderCount × 32-byte null-padded strings   │ │
  │                                                │ │
  │  Bones                                         │ │
  │  • boneCount × 64-byte entries                 │ │
  │  • each: position, orientation, offset,        │ │
  │          parent index, joint ID                │ │
  │                                                │ │
  │  Vertex Sets                                   │ │
  │  • per set: count × stride raw vertex data     │ │
  │  • type 0: rigid     (32B per vert)            │ │
  │  • type 1: 1-bone    (36B per vert)            │ │
  │  • type 2: weighted  (48B per vert)            │ │
  │                                                │ │
  │  Index Buffer                                  │ │
  │  • indexCount × uint16                         │ │
  │                                                │ │
  │  Mesh Groups ──────────────────────────┐       │ │
  │  • meshGroupCount × 56-byte entries    │       │ │
  │  • shaderIndex ────────────────────────┼───────┘ │
  │  • strip/list index ranges into ───────┼─→ Index Buffer
  │  • baseVertex offset into ─────────────┼─→ Vertex Sets
  │  • vertexSetIndex ─────────────────────┼─→ Vertex Sets
  │  • aBones[20] bone palette ────────────┼─→ Bones
  │                                        │       │
  └────────────────────────────────────────┴───────┘
                    │
                    │ shaderIndex → shader name string
                    ▼
              NSA material lookup → DDS texture path
```

### NOD ↔ NAD Bone Mapping

NAD bone tracks target NOD bones by index:

```
  NOD skeleton              NAD tracks
  ┌──────────────┐          ┌──────────────────────┐
  │ bone[0] root │◀─────────│ track: boneNumber=0  │
  │ bone[1]      │◀─────────│ track: boneNumber=1  │
  │ bone[2]      │          │   (no track = static) │
  │ ...          │          │ ...                   │
  │ bone[42]     │◀─────────│ track: boneNumber=42 │
  └──────────────┘          └──────────────────────┘

  Track types:
  • TRACK_ROTATION (0) — euler angles (pitch/yaw/roll in degrees)
  • TRACK_POSITION (1) — translation offset

  Keyframe evaluation (cubic polynomial):
    v(t) = value + t·b + t²·c + t³·d
```

### NOD ↔ NOC Collision Pairing

Almost all NOD render models have a corresponding NOC file that defines the
collision geometry used by the physics engine.  The two files share the same
model name but live in different asset directories:

```
  3D/Models/<name>.nod        render mesh (detailed, textured)
  3D/Collision/<name>.noc     collision mesh (simplified, untextured)
```

NOC contains its own vertex/index data — a simplified, lower-poly version of
the NOD geometry purpose-built for physics.  There is no explicit link in the
file data; they are paired by filename convention at load time
(`AssetBuilder` strips the `.nod` extension and probes for a matching `.noc`).

**Entity collision is mesh-based, not primitive-based.**  Entity models use
their NOC trimesh for physics collision (`RigidBody::addTriMesh`), not
capsules or spheres.  The only capsule collider in the engine is the **player
character controller** (Nova), which is a dynamic body and does not have a
NOC file — it uses an ODE capsule for character movement and ground detection.

For the rare entities that lack a NOC file, the physics system falls back to
an axis-aligned box derived from the model's bounding box
(`RigidBody::addBox` with extents from `bbox_min`/`bbox_max`).

#### Debug Visualization Mapping

| Toggle | Key | What it shows |
|--------|-----|---------------|
| **Collision debug** | `C` | NOC mesh wireframes — cyan (level NIL sectors), green (entity composite, world-space), magenta (per-entity, model-local) |
| **Physics debug** | `P` | Actual ODE physics shapes — capsules, boxes, trimesh AABBs, velocity vectors |

The collision debug (C) shows the **source collision geometry** from the asset
files.  The physics debug (P) shows the **runtime ODE shapes** that the
physics engine actually uses for simulation — these include the player capsule,
NOC trimeshes (rendered as ODE AABBs), and AABB-fallback boxes.

### NMB Bundle Structure

NMB packs multiple NOD-equivalent entries into a single file:

```
  NMB file:
  ┌─────────────────────────────────────────┐
  │ Header: version, modelCount, totalMem   │
  │                                         │
  │ Entry[0] ─────────────────────────────┐ │
  │  • compact header (5B)                │ │
  │  • name (32B)                         │ │
  │  • vertex sets  (same types as NOD)   │ │
  │  • index buffer                       │ │
  │  • shader names → NSA lookup          │ │
  │  • bones (26B each, compact NOD)      │ │
  │  • mesh groups + bone remap tables    │ │
  │  • markers                            │ │
  │                                       │ │
  │ Entry[1] ─────────────────────────────┤ │
  │  • ...same structure...               │ │
  │                                       │ │
  │ Entry[N-1] ───────────────────────────┘ │
  └─────────────────────────────────────────┘
          │
          │ paired by filename stem
          ▼
  NNB file:
  ┌─────────────────────────────────────────┐
  │ Header: version, animCount, dataSize    │
  │                                         │
  │ Entry[0]: name (64B) + offset           │──→ .nad file reference
  │ Entry[1]: name (64B) + offset           │──→ .nad file reference
  │ ...                                     │
  └─────────────────────────────────────────┘
```

---

## NSA Material Properties

The NSA format is the central hub connecting geometry to textures and surface behavior. Each material entry contains:

| Property | Type | Meaning |
|----------|------|---------|
| `texture` | string | Primary diffuse texture name (→ DDS) |
| `texture2` | string | Secondary/alternate texture name (→ DDS) |
| `glow` | string | Emissive overlay map name (→ DDS) |
| `detail` | string | Tiled detail texture name (→ DDS) |
| `envmap` | string | Environment reflection map name (→ DDS) |
| `shaderType` | string | Xbox shader program identifier |
| `intensity` | float | Glow intensity scalar |
| `detailscale` | float | Detail UV tiling factor |
| `pageflip` | int | Animated texture frame count |
| `blendMode` | string | Alpha blending mode |
| `alphaTest` | string | Alpha test enable/threshold |

### Xbox Shader Types in NSA

The `shaderType` property identifies which Xbox vertex/pixel shader program pair the original engine used. These define the intended surface behavior:

| shaderType value(s) | Xbox shader source | Surface behavior |
|---|---|---|
| *(none/default)* | `default*.xvu` | Standard lit diffuse |
| `glossenvmap`, `detail`, `bumpenv`, `envmap`, `boxproject` | `Envmap*.xvu + bumpreflect.ps` | Specular + environment reflection |
| `glow`, `scanline` | `selfglow.xpu` | Self-illuminating emissive |
| `fullbright`, `sky`, `add`, `halo`, `lightvolume`, `pageflip` | `fullbright*` | Unlit texture color |
| `ghostsuit`, `cloak` | `ghostsuit.xpu + cloak*.xvu` | Fresnel rim + cloaking transparency |
| `glass`, `etchedglass`, `etchedmasked`, `fog`, `fogcolor` | `glass*` | Transparent glass + Fresnel edge |
| `bump` | `bump*.xvu + emBump.ps` | Normal/bump mapped |
| `metalgloss`, `metalenvmap` | `MetalEnvmap*.xvu` | Metallic environment reflection |
| `membrane`, `shield`, `psishieldhit` | `membrane*.xvu + shield.ps` | Energy shield effect |
| `lava` | `lava.xpu` | Scrolling UV + emissive glow |
| `water`, `bumpwater` | `waterNM*.xvu + waterNM.ps` | Animated water surface |
| `alphadissolve`, `dissolveglow` | `alphadissolve.ps` | Noise-based alpha dissolve |

### Blend Mode Classification

The `shaderType` value also implies the intended transparency mode:

| Blend mode | shaderType triggers |
|---|---|
| **Opaque** | default, most standard materials |
| **Masked** | `masked`, `etchedmasked` |
| **Additive** | `glow`, `add`, `fullbright`, `halo`, `lava`, names ending in `_glo` |
| **Alpha blended** | `alpha`, `glass`, `water`, `membrane`, `fog` |
| **Not rendered** | `sky`, `portal`, `ge_shadow`, `invis`, `video`, names ending in `_bink` |

---

## NIL Vertex Data Layout

NIL level vertices encode baked lighting from the Xbox D3DCOLOR format:

### Raw NIL Vertex (36 bytes)

| Offset | Size | Field | Encoding |
|---|---|---|---|
| 0 | 12 | position | float[3], D3D Z-up world space |
| 12 | 12 | normal | float[3], unit vector |
| 24 | 4 | diffuse | uint32 Xbox D3DCOLOR (BGRA packed) |
| 28 | 8 | uv | float[2], texture coordinates |

### Xbox D3DCOLOR Decoding

The `diffuse` field is Xbox D3DCOLOR — BGRA byte order packed into uint32:

```
  Byte layout:  [B][G][R][A]
  Bit layout:   AAAA AAAA RRRR RRRR GGGG GGGG BBBB BBBB

  R = (diffuse >> 16) & 0xFF
  G = (diffuse >>  8) & 0xFF
  B = (diffuse >>  0) & 0xFF
```

Xbox uses **2× overbright** convention: byte value 128 = 1.0 intensity, 255 ≈ 2.0 intensity. To recover linear color: `channel = byte / 128.0`.

### NIL Surface Group Vertex Types

| vertType | Meaning | Extra data |
|---|---|---|
| 0 | Standard (dynamic lighting) | Vertex colors ignored or absent |
| 1 | With TBN basis | Additional 36 bytes: tangent + binormal + normal |
| 2 | Baked vertex color | Vertex colors contain pre-baked lighting |

---

## NOD Vertex Data Layouts

NOD stores vertices in three formats depending on skinning requirements:

### Type 0 — Rigid (32 bytes/vertex)

| Offset | Size | Field |
|---|---|---|
| 0 | 12 | position (float[3]) |
| 12 | 12 | normal (float[3]) |
| 24 | 8 | uv (float[2]) |

No bone assignment — vertex is static or transformed as a whole object.

### Type 1 — Single Bone (36 bytes/vertex)

| Offset | Size | Field |
|---|---|---|
| 0 | 12 | position (float[3]) |
| 12 | 12 | normal (float[3]) |
| 24 | 8 | uv (float[2]) |
| 32 | 4 | boneIndex (uint32, local palette index) |

Vertex is rigidly attached to one bone. Weight is implicitly 1.0.

### Type 2 — Weighted (48 bytes/vertex)

| Offset | Size | Field |
|---|---|---|
| 0 | 12 | position (float[3]) |
| 12 | 12 | normal (float[3]) |
| 24 | 8 | uv (float[2]) |
| 32 | 12 | weights (float[3], fourth weight = 1.0 − sum) |
| 44 | 4 | boneIndices (uint8[4], local palette indices) |

Up to 4 bones influence each vertex. Indices reference the mesh group's local bone palette (`aBones[20]`), which maps to global skeleton indices.

### Bone Palette Indirection

```
  vertex.boneIndex[i]         local palette index (0–19)
        │
        ▼
  meshGroup.aBones[local]     global skeleton bone index (0–boneCount)
        │
        ▼
  NOD bone array[global]      rest pose position + orientation + parent
```

---

## NOD Index Buffer Encoding

NOD mesh groups reference two index regions within the shared index buffer:

```
  Index Buffer (shared across all mesh groups)
  ┌──────────┬───────────┬──────────┬───────────┬─────┐
  │ grp0     │ grp0      │ grp1     │ grp1      │ ... │
  │ strips   │ lists     │ strips   │ lists     │     │
  └──────────┴───────────┴──────────┴───────────┴─────┘

  Per mesh group:
  • startIdx          — offset into index buffer
  • numStripIndices   — triangle strip index count
  • numListIndices    — triangle list index count
  • baseVertex        — added to each index value
```

**Triangle strips** use degenerate triangles (repeated indices) as separators between sub-strips. Conversion to triangle lists requires:
- Walking in groups of 3 with a sliding window
- Alternating winding order on even/odd triangles
- Discarding degenerate triangles (any two indices equal)

---

## NAD Animation Data Model

NAD bone tracks define motion as cubic polynomial keyframe curves:

```
  NAD file
  ┌───────────────────────────────────────────────┐
  │ Header: version=4, numBoneTracks, duration    │
  │                                               │
  │ BoneTrack[0]:                                 │
  │   boneNumber=5, type=ROTATION, numKeys=12     │
  │   ┌─────────────────────────────────────────┐ │
  │   │ Key[0]: frame=0.0                       │ │
  │   │   value=(10.5, -3.2, 0.0)  ← base      │ │
  │   │   b=(0.1, 0.0, -0.5)       ← linear    │ │
  │   │   c=(0.0, 0.02, 0.0)       ← quadratic │ │
  │   │   d=(0.0, 0.0, 0.001)      ← cubic     │ │
  │   │                                         │ │
  │   │ Key[1]: frame=5.0                       │ │
  │   │   ...                                   │ │
  │   └─────────────────────────────────────────┘ │
  │                                               │
  │ BoneTrack[1]:                                 │
  │   boneNumber=0, type=POSITION, numKeys=3      │
  │   ...                                         │
  │                                               │
  │ KeyTags: numKeyTags entries                   │
  │   (tagType, frameNum) — animation events      │
  │                                               │
  │ ShapeTracks: numShapeTracks entries            │
  │   (groupNum, shapeNum, weight keyframes)       │
  └───────────────────────────────────────────────┘

  Evaluation at time t within keyframe span:
    v(t) = value + t·b + t²·c + t³·d

  Rotation tracks: euler angles in degrees (pitch, yaw, roll)
  Position tracks: translation offset from rest pose
```

### Bone Hierarchy Evaluation Order

```
  For each bone (parent-first order):
    1. Look up curRotation[i] from NAD rotation track (or zero if no track)
    2. Look up curTranslation[i] from NAD position track (or zero if no track)
    3. Build local rotation matrix from euler PYR (pitch/yaw/roll)
    4. Combine: localMat = rotation × translate(restPos + curTranslation)
    5. If root (parent=0xFF): globalPose = localMat
       Else: globalPose = globalPose[parent] × localMat
    6. Final: boneMat = globalPose × translate(-invRootOffset)
```

---

## NOT Template Hierarchy

NOT organizes ~1224 entity templates in an inheritance tree:

```
  Root templates (flags=47):
  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ _ACTOR   │  │ _EFFECT  │  │_PROJECTILE│  │  _PROP   │
  └────┬─────┘  └────┬─────┘  └────┬──────┘  └────┬─────┘
       │             │             │               │
       ├─ Marine     ├─ Explosion  ├─ Bullet       ├─ Crate
       ├─ Zergling   ├─ Sparks     ├─ Missile      ├─ Door
       ├─ Firebat    └─ ...        └─ ...          └─ ...
       └─ ...

  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ _REGION  │  │ _SENSOR  │  │_UIELEMENT│
  └────┬─────┘  └────┬─────┘  └────┬─────┘
       │             │             │
       ├─ trigger_   ├─ motion_    ├─ healthbar
       ├─ camera_    ├─ sound_     ├─ ammo_count
       └─ ...        └─ ...        └─ ...

  Each child template:
  • parent name (32 bytes) — inherits all parent properties
  • numProperties — additional/overridden properties
  • Properties: ID + 32-byte value, or boolean (5B), or comment string
```

Template names in NOT match entity type names found in NSD scene data — this is how the engine resolves entity capabilities and default property values.

---

## Coordinate System

All spatial data across all formats uses **Direct3D left-handed Z-up**:

```
       Z (up)
       │
       │
       └── Y (forward)
      /
     X (right)
```

| Axis | Direction |
|------|-----------|
| X | Right |
| Y | Forward (into screen) |
| Z | Up |

Positions, normals, bone offsets, entity placements, region bounds, and camera keyframes all use this convention.
