# NODSurvey and NODExport

Two command-line tools for external use. They read game assets with the SDK
parsers and write JSON. Other tools, such as web viewers, can read that JSON
directly. They write only the output files you name.

`<build>` is the root of an extracted build (it holds `3D/`, `Materials/`,
`Misc/`). `<folder>` is one asset folder, for example `3D/Models`.

Both tools print usage when the arguments are wrong.

## Exit codes

| Code | NODSurvey | NODExport |
|---|---|---|
| 0 | Every file read cleanly | Exported |
| 1 | A file was refused or did not fit the layout | Input refused; `out.json` holds `{"format","error"}` |
| 2 | Usage or output error | Usage or output error |

## NODSurvey

Reads every file of one kind and reports what it holds and where it does not
fit the known layout. Parser log lines go to the console. The report goes to
`<out.json>` only.

```
NODSurvey nod <folder> <out.json>
NODSurvey nad <folder> <out.json>
NODSurvey nsa <build>  <out.json>
NODSurvey nms <build>  <out.json>
NODSurvey not <build>  <out.json>
NODSurvey dds <build>  <out.json>
```

| Mode | Reads | Reports | Exit 1 when |
|---|---|---|---|
| `nod` | `*.nod` in folder | versions, vertex types, group bytes, bone palettes, structure faults | file refused or bytes left over |
| `nad` | `*.nad` in folder | tracks, keys, NaN keys, frame order, curve joins (frames / unit / seconds) | file refused or bytes left over |
| `nsa` | `Materials/` | archive order, braces, dropped properties, textures, how each model material name is found | block not closed, stray `}`, property dropped on a drawn material |
| `nms` | `3D/Motions`, `3D/Anims` | flags, clip match: exact, by case, by added `.nad`, missing | file refused, short line, dropped flag, clip names that differ only by case |
| `not` | `Misc/Templates/Global.not` | `basedon` resolution, model (id 4) and motion set (id 13) match | template refused, bad string, incomplete or unknown property |
| `dds` | every `.dds` under build | formats, mips, cubes, volumes, payload size: short, exact or over | file refused, size not computable, payload short |

The match classes only record what is on disk. They do not claim what the
engine does.

## NODExport

Exports one asset as data a renderer can load.

```
NODExport nod       <in.nod>  <out.json> <out.bin>
NODExport nad       <in.nad>  <out.json>
NODExport materials <build>   <names.txt> <out.json>
NODExport catalog   <build>   <out.json>
```

An input is refused, not partly exported, if it has the wrong version,
unexplained bytes, or a structure fault.

### `nod`

Accepts version 10 only. Exports detail level 0. Strips become triangles and
degenerate triangles are dropped. Values keep the file's own axes.

`out.bin`, little-endian:

1. Every vertex of every vertex set, in set order, 52 bytes each:
   `float3 position`, `float3 normal`, `float2 uv`, `uint8[4] bones` (model
   bone numbers), `float[4] weights`.
2. `uint32` triangle indices into that vertex array.

`out.json`: `boundsMin`, `boundsMax`, `materials`, `bones` (`parent`,
`position`, 3x3 `orientation`, `offset`), `vertexStride`, `vertexCount`,
`indexOffset`, `indexCount`. Each entry of `groups` has `material`,
`vertexType`, `firstVertex`, `vertexCount`, `firstIndex`, `indexCount`,
`droppedTriangles`, `bones`.

### `nad`

Accepts version 4 only. `out.json`: `length`, `framesPerSecond` (30),
`shapeTracks`. Each entry of `tracks` has `bone`, `kind` (0 turn, 1 position),
and `keys`. Each key is `[frame, value xyz, b xyz, c xyz, d xyz]`: a cubic
from that key. Each entry of `tags` has `type` and `frame`. NaN is written as
`null`.

### `materials`

`names.txt` holds one model material name per line. Each name is looked up
in this order:

1. `archive`: the first definition in a `.nsa`, in `Materials/shaderlist.txt`
   order, then the unlisted archives by name.
2. `nas`: `Materials/**/<name>.nas`.
3. `texture`: a texture of the same name.
4. `none`.

Textures are searched in `3D/Materials` first, then the folders listed in
`MaterialDirs.txt`. Names are matched without case. Per name the output has
`step`, `source`, `line`, `properties` (in file order), and `textures`. The
`textures` object maps each texture key to a build-relative path, or `null`
if missing.

### `catalog`

A listing of the build for an asset browser:

- `templates`: name, model, motion set, script, type, scale, resolved
  through `basedon`.
- `motionSets`: every `3D/Motions/*.nms` with its entries.
- `models`: every `3D/Models/*.nod` with its counts and vertex types.

Refused items go to `refusedTemplates`, `refusedMotionSets` and
`refusedModels`. The rest is still exported.

## Parser library additions

These are reusable outside the tools:

- `NODCheck`: model structure checks (`nodFaults`, `nodGroupVertexCount`, `nodGroupEmpty`).
- `NODMaterialLibrary`: material and texture lookup for a build.
- `bytes_read` on NOD and NAD to detect leftover bytes. NOD also gets
  `blend_count`, `color_count` and `mesh_group_offset`.
- NSA: line numbers, comment and brace counts, dropped and nested property counters.
- NMS: `short_lines` and `dropped_flags` counters.
