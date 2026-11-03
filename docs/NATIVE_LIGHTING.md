# Native NIL Lighting: Technical Design and Validation

Implementation snapshot: 2026-10-23.

This document describes the CPU direct-lighting implementation shared by the
open NOD SDK world compiler and the StarCraft: Ghost GtkRadiant authoring
profile. It separates verified native behavior from compiler policy and from
features that remain unimplemented.

## 1. Problem and scope

Freshly compiled worlds previously had white vertex colors, zero global and
ambient lighting, and no sector static lights. World brushes therefore appeared
fullbright while moving characters had no authored static lights to select.
Correct model placement or material loading alone cannot supply these inputs.

The implemented solution has two complementary outputs:

1. CPU-baked world vertex colors, including brush occlusion.
2. Native sector point-light records for moving characters.

This is not a Quake-style lightmap texture-atlas compiler. No GPU renderer,
texture lightmap allocation, or graphics device is required to compile a level.
The existing NIL v35 primary vertex pool and sector-light framing carry the
results. Global lighting and header ambient remain zero unless explicitly
enabled through the extended export controls.

The implementation does not add lights automatically or rewrite the source MAP.
Maps without authored lights retain the previous white, zero-light baseline.

## 2. Authoring contract

### Light entity

Use the MAP-only point entity `nod_static_light`, available under **nod** in
Radiant's entity browser. It is not a native NOT template or an NSD placement.
Using a separate classname avoids collisions with native template names and
the editor's generic Quake `light` behavior.

| Key | Type and accepted values | Default |
|-----|--------------------------|---------|
| `classname` | Exactly `nod_static_light` | Set by placement |
| `origin` | Three finite world-coordinate floats; required | Placement position |
| `_color` | Three finite floats in 0..1; at least one positive channel | `1 1 1` |
| `light` | Finite intensity, strictly greater than 0.2 and at most 1024 | `8` |
| `radius` | Finite attenuation reference distance, 0.01..65536 world units | `10` |

Only these five light properties are supported. Unknown or duplicate light
properties, brush-bearing lights, malformed numbers, extra numeric components,
and more than 64 lights fail explicitly. In particular, unsupported `target`
or `spawnflags` keys are not silently interpreted.

Numbers are parsed using the classic locale. No coordinate rescaling or
conversion to photometric units is performed.

Example entity fragment, to be added after a valid brush worldspawn:

```text
{
"classname" "nod_static_light"
"origin" "0 0 5"
"_color" "1 0.75 0.5"
"light" "8"
"radius" "10"
}
```

Place lights in empty space, not inside solid brushes or directly on surfaces.
The fragment above is not a complete compilable map.

### World sampling

Optional worldspawn `nod_light_spacing` sets the maximum refined triangle-edge
length:

| Setting | Default | Accepted range |
|---------|---------|----------------|
| `nod_light_spacing` | 4 world units | 0.01..1024 world units |

For example, add `"nod_light_spacing" "2"` to the existing worldspawn entity.
Smaller spacing samples gradients and shadow boundaries more finely but
increases render and collision triangle counts.

Spacing may be retained without point lights, including when the dialog supplies
sun or ambient instead. Without an active light source, automatic lighting mode
keeps the old unlit baseline and does not refine geometry.

`radius` is a falloff reference distance, not a hard influence cutoff. The
native type-0 graphics setup uses a separate fixed range of 9999. Values of
`radius` above that range are accepted as curve parameters; they do not extend
the native graphics range.

### Extended export dialog and compiler overrides

The dialog follows the reference export-window layout: Geometry and Lighting
groups on the left, Presets on the right, and status, progress and diagnostic
output below. The existing workspace/tool/asset/output paths remain available.

| Control | Actual behavior |
|---------|-----------------|
| Export geometry | Rebuild and publish NIL |
| Compute lighting | Enable authored/explicit lighting; also rebuilds NIL even if geometry export is unchecked |
| Pathing info | Compile and publish NPD |
| Export static objects / scenes | Compile NSD template placements as one output |
| Oversample | Four deterministic surface samples per rendered vertex instead of one |
| Brush shadows | Enable or disable brush occlusion for point and directional lights |
| Override MAP light resolution | Override worldspawn spacing for this conversion only |
| Light resolution | Logarithmic slider plus exact sample-spacing field in world units |
| Phong angle | Area-weighted normal smoothing within the angle threshold; 0 preserves hard normals |
| Moon / Sun light | One native directional global light plus matching CPU surface illumination |
| Ambient light | Constant CPU surface contribution plus verified native header ambient for characters |

Phong here means smoothing of normals, including the emitted render normals.
It does not add a specular Phong material/shader.
Oversampling averages the vertex sample with three nearby barycentric samples
inside incident triangles. It improves gradient/shadow sampling, but is not
area-light soft shadows or indirect bounce.

Sun and ambient UI colors use integer RGB 0..255. Their intensity controls use
percent, with 100% = a normalized multiplier of 1. These are new SDK authoring
units, not a claim of matching every original Embrace numeric unit. The default
sun direction is `(-1, 2, -4)`, intensity 100%; ambient defaults to 35%.
Both sources are disabled initially. Direction is normalized and denotes the
direction light travels.

| Preset | Outputs and quality |
|--------|---------------------|
| Full Export | NIL, NSD, NPD; four samples, shadows, Phong 89 degrees, MAP/default spacing |
| Fast Lighting | NIL only; one sample, shadows, spacing override 8, no smoothing |
| Geometry Only | NIL with lighting disabled; NSD/NPD preserved |
| Objects Only | NSD only; NIL/NPD preserved |

Presets retain user-selected sun/ambient colors, intensities and enable flags;
lighting-disabled presets temporarily ignore those settings. Fast Lighting is
a lower-cost full NIL rebuild, not an incremental modification of an old NIL.
Initial dialog quality uses four samples and Phong 89 degrees; CLI defaults
remain one sample and no smoothing for backward compatibility.

Compiler options are passed as individual arguments, never shell text:

```text
--lighting auto|on|off
--light-spacing N
--light-samples 1|4
--light-shadows on|off
--light-phong DEGREES
--sun-light R G B INTENSITY DX DY DZ
--ambient-light R G B INTENSITY
```

CLI RGB is normalized 0..1; sun/ambient intensities are positive multipliers up
to 1024. Duplicate options, invalid ranges, nonfinite numbers and zero sun
direction fail without publication. `auto` bakes only when a source exists;
`on` explicitly permits a dark bake without sources; `off` emits the original
white, zero-light output. Sun/ambient CLI options combined with `off` are rejected.
None of these overrides edits the source MAP.

The current native compiler still emits one sector with unified brush
collision. The reference's independent sector/detail/sector-mark switches are
not presented as nonfunctional controls. Static objects and scenes share the
existing NSD placement pipeline rather than unsupported separate export passes.

## 3. Verified native representation

All offsets below refer to the supported NIL v35/native executable contract.
Native code addresses are specific to the runtime accepted by the test
harness, not portable entry points for arbitrary game builds.

### Header and primary vertices

| Location | Size | Meaning |
|----------|------|---------|
| NIL header byte 8 | 72 bytes | Global light record |
| NIL header byte 80 | 12 bytes | Three ambient floats |
| Primary vertex byte 0 | 12 bytes | Position |
| Primary vertex byte 12 | 12 bytes | Normal |
| Primary vertex byte 24 | 4 bytes | Packed diffuse D3DCOLOR |
| Primary vertex byte 28 | 8 bytes | UV |

The primary vertex stride is 36 bytes. The native reader preserves diffuse
words unchanged in the vertex buffer.

The baker stores numeric `0xFFRRGGBB`, with opaque alpha. In the little-endian
stream its bytes are B, G, R, A. This describes storage and the baker's packing
policy, not a claim about final display gamma or every material's shading.

### Sector framing

The relevant variable-length portion after sector indices is:

```text
uint8   thing_count
byte    things[thing_count * 68]
uint16  static_light_count
byte    static_lights[static_light_count * 72]
        existing environment-reference fields and sector tail
```

Fresh worlds currently write zero things, then the authored static-light count
and records, followed by the existing zero-reference/tail fields. Lighting
does not introduce another image section or a texture atlas.

Records previously named opaque `SurfThing` entries in parser terminology are
native static `tRenderLight` records. Older references to lightmap patches or
an atlas must not be used to infer a conventional lightmap layout from these
fields; the separate secondary vertex pool is not such an atlas.

### Static point-light record

Each record is exactly 72 bytes, serialized little-endian:

| Byte | Type | Meaning and generated value |
|------|------|-----------------------------|
| 0 | uint32 | Flags/metadata; generated as 0 |
| 4 | uint32 | Native light type; point = 0, directional global = 2 |
| 8 | uint32 | Style index; generated as 0 to bypass style lookup |
| 12 | float[3] | Authored RGB multiplied by 255 |
| 24 | float | Render intensity; initialized from authored `light` |
| 28 | float | Selection/base intensity; initialized from authored `light` |
| 32 | float[3] | Direction; generated as `(0, 0, -1)` |
| 44 | float[3] | Authored world position |
| 56 | float[2] | Cone-related fields; generated as zero for point lights |
| 64 | float | Authored `radius`; not a type-0 selection cutoff |
| 68 | float | Derived falloff coefficient |

Full flag meanings, light-style authoring and cone-light behavior are not
established by the point-light implementation.

The native sector reader stores the count at sector offset `0x10` and the
record-array pointer at sector offset `0xE4`. Object static-light gathering
selects from this same array. Colored vertices alone cannot replace it.

## 4. Native equations and CPU lighting policy

Let `I` be authored intensity, `R` authored radius, and `q = 0.2` the compiler's
chosen cutoff parameter for the native falloff calculation.

```text
f = (I - q) / (R * (1 + 1.2 * R) * q)
A(d) = I / (1 + f*d + 1.2*f*d*d)
```

The falloff equation matches `worldLighting_CalcLightFalloff`. Without the
Lambert or color factors, `A(R)` is approximately `q`, subject to floating-point
rounding. This is why radius describes the attenuation curve rather than a
hard truncation.

For the defaults `I = 8`, `R = 10`, the stored coefficient is approximately
`f = 0.3`.

### Native character-light selection

For native point type 0, the observed selection weight is:

```text
selection_weight = max(native_RGB) / 255 * A(d_native)
```

The original engine uses an approximate distance calculation based on its
inverse-square-root routine, including the `0x5F3759DF` seed and one Newton
step. The CPU surface baker instead uses double-precision Euclidean distance.
Consequently, native selection and CPU sample values need not be bit-identical.

The tested object-gathering path requests up to four lights. It is a runtime
selection mechanism, not a bake of all contributing lights or brush shadows.

### Native graphics boundary

For style 0, `renderLight_AddLight` derives field 24 from the supplied weight
times field 28. The native point-light graphics setup produces:

```text
diffuse_RGB = native_RGB * render_intensity / 255
attenuation = (1, f, 1.2*f)
range       = 9999
```

These values are verified by executing the original setup routines and
capturing their graphics API arguments. No GPU image or complete game frame
is generated by this test.

### CPU surface bake

For a rendered vertex position `P`, its normalized surface normal `N`, and a
light position `L`:

```text
D = L - P
d = length(D)
lambert = max(0, dot(N, D) / d)
contribution[channel] = color[channel] * lambert * A(d)
```

The point-light contribution is omitted when:

- `d <= 1e-8`, avoiding division by zero at a coincident position;
- `d > 9999`, matching the native point-light graphics range;
- the Lambert factor is zero; or
- an original world-brush triangle blocks the shadow segment.

Contributions from all authored lights are accumulated. Each final channel is
clamped to at most 1, multiplied by 255, rounded with `lround`, and packed into
the diffuse word. There is no exposure control, gamma transform or HDR storage.
Explicit ambient is added once per vertex;
directional direct light and point contributions are averaged across the
selected number of samples. There is still no indirect bounce.

### Ambient evidence and deliberate omission

The native `rlLight_SetAmbient` prefix multiplies incoming channels by
`0.16326530277729034`, from float bits `0x3E272F05`. It does not simply divide
by 255. The complete header-to-object ambient path is now covered by the extended native oracle:
the header floats are copied to `WORLD+0x108`, object drawing takes a
componentwise maximum with game-sector ambient at `+0xE8`, then the complete
setter scales and rounds directly into packed 0..255 ARGB channels.

For normalized ambient color times authored intensity `a`, the compiler writes:

```text
header_ambient = clamp(a, 0, 1) * 255
surface_ambient = clamp(a, 0, 1) * 0.16326530277729034
```

Ambient intensity is strength before the engine's attenuation, not a target
post-attenuation brightness. The native character shader path at
`cShader::SetVertexShaderLights` (`0x005FCB50`) reads `rlLight_ambientRGBA`,
multiplies by approximately `1/255` (constant at `0x007A9F64`), and uploads it
in vertex constant c116. Thus its RGB contribution is approximately
`clamp(a,0,1) * 0.16326530277729034`, matching the CPU surface contribution.
No enabled ambient source means a zero header.

The first extended-export version incorrectly divided the header value by
the native attenuation constant. This canceled intentional native attenuation
and made ambient approximately 6.125 times stronger. White ambient at 35%
previously produced header RGB `546.65625`; the corrected header is `89.25`,
yielding shader RGB approximately `0.05714`, not `0.35`. A packed-ARGB-only test
did not establish the complete character-shader contract; regression coverage
now captures the actual native c116 upload as well.

NIL files compiled with the incorrect mapping must be regenerated. This fix
does not rewrite existing outputs or original assets. Other lighting settings
and source MAP entities do not need to change.

The sun uses a type-2 global record with RGB times 255, both intensity fields
initialized from the multiplier, and normalized direction. CPU illumination
uses `max(0, dot(normal, -direction))` with optional brush shadow rays. The
sector flags are `5 | 0x20` when sun is enabled, otherwise the old value `5`.
Native object global-light gathering requires that sector bit, a non-excluded
object definition and positive object fade. Native LOS points opposite the
stored direction. One sun/moon record is supported, not independent sun and
moon sources.

## 5. Geometry refinement and occlusion

### Conforming subdivision

The existing compiler requires matching render and collision triangle ordering.
The lighting implementation preserves this contract by refining their shared
geometry instead of maintaining a separate render-only tessellation.

1. Copy the post-CSG world geometry.
2. Find the globally longest triangle edge exceeding the requested spacing.
3. Insert one shared midpoint.
4. Bisect every triangle sharing that edge, preserving winding.
5. Rebuild each surface's contiguous triangle span.
6. Repeat until every edge satisfies spacing or a limit is reached.
7. Regenerate collision MOPP for the refined mesh.

Splitting all users of an edge avoids subdivision-created T-junctions.
Original surface boundary windings remain unchanged. An internal
`Geometry::lighting_refined` marker permits render construction to add newly
referenced edge/interior vertices while preserving the old rejection of
out-of-winding triangle references for ordinary, unrefined geometry.

New render vertices use the existing surface normal and classic UV projection.
Hard-normal and UV seams remain separate render vertices even where collision
positions are shared. Baseline winding-corner order remains unchanged.

Refinement does not expand the world bounds. Automatic physics bounds remain
the world box plus outward padding; lights do not enlarge that collision box.

### Shadow rays

Occlusion uses the original post-CSG, pre-subdivision world mesh. Subdivision
adds lighting samples but need not increase the number of shadow occluders.

- Calculations use double precision.
- The ray starts at `P + N * 0.0001` world units to reduce self-intersection.
- A segment/triangle AABB rejection runs before the intersection test.
- Triangle intersections are two-sided.
- The determinant threshold is `1e-14`; barycentric tolerance is `1e-9`.
- A hit blocks the light when segment parameter `t` is strictly between
  `1e-8` and `1 - 1e-8`.
- Every visited occluder triangle consumes budget, including AABB rejections.

The budget is shared across the whole bake. Exhaustion is an explicit error,
not a successful bake with missing light. The fixed bias and tolerances are
implementation policies, not recovered native constants.

There is no BVH for bake rays in this first pass. Cost is broadly proportional
to rendered samples times contributing lights times original occluder triangles,
with early exits for back-facing, out-of-range and blocked samples.

## 6. Compiler integration and failure behavior

The `MAPtoNIL` pipeline is:

```text
parse MAP
  -> validate lighting settings
  -> build world CSG geometry
  -> retain original geometry as occluders
  -> refine shared geometry when lights exist
  -> compute/validate physics bounds
  -> resolve materials and build render geometry
  -> bake colors and serialize point-light records
  -> assemble NIL and collision/group mappings
  -> validate compiled collision framing
  -> atomically publish the NIL
```

`RenderVertex::diffuse` defaults to white. `RenderGeometry::static_lights`
holds the serialized point records; global and ambient fields carry explicit
directional/header settings. The sector writer validates record framing and
count, writes each vertex's diffuse word, then emits the static-light array.

`MAPtoNSD` runs the same authored-light validation and excludes these helpers
from template placements. It permits the lighting spacing worldspawn key;
this is not general support for arbitrary worldspawn settings.
`MAPtoNPD` remains the manual-navigation compiler; no light generates waypoints.

Geometry refinement and baking work on temporary result objects and only
replace their output objects on success. CLI publication occurs after image
validation. Invalid lights, subdivision overflow, precision collapse, missing
materials, or ray-budget exhaustion do not replace a previous NIL.

The editor's **Bsp > Convert NOD level...** dialog saves current edits and
runs the selected external compilers from one MAP snapshot. It stages the outputs
before publication and attempts rollback on publication failure. This is not
a crash-atomic three-file transaction. Unrelated invalid waypoint IDs or NSD
placements can still fail the combined conversion even when lighting is valid.
Unselected outputs are neither backed up nor replaced; only selected tools
are required to exist.

### Enforced lighting-related limits

| Resource | Limit or condition |
|----------|--------------------|
| Authored point lights | 64 |
| Refined world triangles | 4096, matching the current MOPP subset |
| Shared mesh vertices | 65535 |
| Render vertices, including seams | 65535 |
| Bake shadow triangle visits | 64,000,000 total |
| Static-light byte array | Multiple of 72 bytes; at most 64 records |
| Midpoint construction | Must not collapse to an endpoint in float precision |

Existing material, surface, file-size and native framing limits also apply.
No quality reduction or automatic sample-spacing increase is performed.

## 7. GtkRadiant integration

The SCGhost profile adds a dedicated early creation path for
`nod_static_light`. Other game profiles retain their existing behavior.

- The class remains visible despite the usual hiding of internal `nod_` classes.
- Its name is reserved against collisions with imported NOT template names.
- Placement requires a fixed-size definition containing the marker
  `MAP-only static lighting`; stale definitions produce an explicit error.
- The marker uses orange color `(1, 0.7, 0.15)` and bounds of +/-0.125 units.
- Creation uses the existing non-destructive `NodPoint_Create` factory.
  Previously selected brushes are not converted into a light.
- New markers are selected, movable, and recorded as `Place NOD light` in Undo.
- Worldspawn spacing is exposed as **Lighting Sample Spacing**.
- Definition changes require editor restart/reload.

The entity definition is maintained in the SDK authoring definitions, bundled
gamepack, staged gamepack and active authoring workspace. Reinstalling a
gamepack must not reintroduce an older definition.

### User workflow

1. Open the updated SCGhost profile.
2. Place **nod > nod_static_light** in an empty room.
3. Start with white color, intensity 8 and radius 10.
4. Save and use **Bsp > Convert NOD level...** with current SDK executables.
5. Inspect the compiler log, then test the published level in the game.
6. Adjust placement, color, intensity and sampling as needed.

There is no baked-light viewport preview in Radiant. Increasing light intensity
can saturate vertex colors; it does not add indirect illumination to occluded
surfaces.

## 8. Implementation map

Paths below are relative to this document.

| Component | Responsibility |
|-----------|----------------|
| [map_lighting.h](../src/tools/common/map_lighting.h), [map_lighting.cpp](../src/tools/common/map_lighting.cpp) | Shared settings, strict parsing, native falloff and 72-byte serialization |
| [world_lighting.h](../src/tools/common/world_lighting.h), [world_lighting.cpp](../src/tools/common/world_lighting.cpp) | Shared-edge refinement, brush shadow rays and diffuse bake |
| [world_geometry.h](../src/tools/common/world_geometry.h) | Shared geometry and refinement marker |
| [world_render.h](../src/tools/common/world_render.h), [world_render.cpp](../src/tools/common/world_render.cpp) | Diffuse/static-light storage and refined vertex UV/normal construction |
| [world_sector.cpp](../src/tools/common/world_sector.cpp) | Native diffuse and static-light framing |
| [maptonil.cpp](../src/tools/map2nil/maptonil.cpp) | Lighting pipeline and protected NIL publication |
| [maptonsd.cpp](../src/tools/map2nil/maptonsd.cpp) | Validation and exclusion from template placement |
| [NILParser.h](../src/parsers/NILParser.h), [NILParser.cpp](../src/parsers/NILParser.cpp), [NILCollision.cpp](../src/parsers/NILCollision.cpp) | Corrected legacy lighting terminology and native framing |
| [nodlightplacement.h](../../GtkRadiant/radiant/nodlightplacement.h), [nodpoint.h](../../GtkRadiant/radiant/nodpoint.h) | Editor point creation and Undo integration |
| [nodexportdialog.h](../../GtkRadiant/radiant/nodexportdialog.h), [nodcompile.h](../../GtkRadiant/radiant/nodcompile.h), [gtkdlgs.cpp](../../GtkRadiant/radiant/gtkdlgs.cpp) | Export layout, presets, numeric/color controls, selected tools, argument forwarding and progress |
| [xywindow.cpp](../../GtkRadiant/radiant/xywindow.cpp), [nodclass.h](../../GtkRadiant/radiant/nodclass.h), [eclass.cpp](../../GtkRadiant/radiant/eclass.cpp), [nodproperties.h](../../GtkRadiant/radiant/nodproperties.h) | Creation routing, discovery, reserved name and spacing UI |
| [entities.def](../src/tools/radiant/entities.def) | SDK authoring definition |

## 9. Native evidence and regression coverage

### Native entry points

| Routine or boundary | Address | Evidence |
|---------------------|---------|----------|
| Sector reader | `0x005F26A0` | Reads sector data; light count read near `0x005F2B62` |
| `tObject::GetStaticLights` | `0x00427690` | Selects generated sector lights |
| `CalculateLightIntensity` | `0x004270E0` | RGB/intensity/falloff selection weight |
| Approximate distance / inverse square root | `0x0042A7E0` / `0x0042A7A0` | Native distance approximation |
| `worldLighting_CalcLightFalloff` | `0x004263E0` | Authored intensity/radius-to-falloff conversion |
| `renderLight_AddLight` | `0x005E4F80` | Style-0 render intensity update |
| `rlLight_AddLight` | `0x00601F10` | Native point-light setup |
| D3D SetLight / LightEnable | `0x006B14E0` / `0x006B17A0` | Captured graphics API boundary |
| `rlLight_SetAmbient` | `0x006022D0` | Ambient prefix scale, stopped before graphics state at `0x0060231D` |
| `AddThingLights` | `0x00427CB0` | Global-light selection and sector/definition gating |
| `UpdateGlobalLightLOS` | `0x004272F0` | LOS toward the negative stored direction |
| `DrawThings` ambient setup | `0x00429490` | Componentwise world/sector ambient maximum passed to the full setter |
| `cShader::SetVertexShaderLights` | `0x005FCB50` | Ambient RGBA normalization and c116 upload through native shader-constant API boundaries |

The Unicorn harness runs the original accepted executable with mocked
file/allocation/graphics boundaries. Selection results and attenuation are not
replaced with mocks. Runtime compatibility is checked by the harness; these
tests require separately supplied original assets, which are not redistributed.

### Focused tests

| Test source | Coverage |
|-------------|----------|
| [test_lighting_native.py](../src/tools/tests/test_lighting_native.py) | Seven native tests: zero-light behavior; record loading and selection; color/intensity scaling; zero contributors; graphics setup; unchanged diffuse upload; ambient prefix |
| [test_extended_lighting_native.py](../src/tools/tests/test_extended_lighting_native.py) | Ten native tests: header copy, directional/global selection and gating, direction/LOS, style scaling, full ambient maximum/scaling/packing and actual character shader ambient upload |
| [test_maptonil.py](../src/tools/tests/test_maptonil.py) | Serialized records, colored samples, black brush shadow sample, spacing, closed oriented edges, determinism, invalid inputs, output preservation, refined native collision rays and lit temporary room copy |
| [test_maptonsd.py](../src/tools/tests/test_maptonsd.py) | Light validation and byte-identical placement output when valid light helpers are added |
| [nod_properties.cpp](../../GtkRadiant/regression_tests/nod_properties.cpp) | Spacing property visibility and display name |
| [nod_templates.cpp](../../GtkRadiant/regression_tests/nod_templates.cpp) | Light-class discoverability |
| [nod_template_placement.cpp](../../GtkRadiant/regression_tests/nod_template_placement.cpp) | Definition guard, defaults, non-destructive placement, retry after failure and actual Undo/Redo |
| [nod_export_dialog.cpp](../../GtkRadiant/regression_tests/nod_export_dialog.cpp), [nod_compile.cpp](../../GtkRadiant/regression_tests/nod_compile.cpp) | GTK presets, control sensitivity, resolution/color conversion, selected-output publication, argument forwarding and rollback |

The extended validation set contains twelve focused SDK suites:
`WorldGeometry`, `WorldRender`, `WorldSector`, `WorldImage`, `MAPtoNPD`,
`MAPtoNSD`, `MAPtoNIL`, `WorldNative`, `LightingNative`, `ExtendedLightingNative`, `PhysicsNative`
and `SolverNative`. Editor property, catalog and placement regressions also
passed. Native world/physics/solver suites additionally protect existing
non-lighting behavior; they are not a claim of full lit gameplay validation.

The selected test room was compiled with a light added only to a temporary
copy. The test checked nonwhite/nonblack output, the triangle limit, native
loading and direct/accelerated collision rays. The source MAP and original
assets were preserved. Existing no-light room output remained byte-identical.

Neither an in-game visual comparison nor a complete rendered frame was
verified during this implementation.

## 10. Build and reproduce validation

From the workspace root, using the existing configured standalone build:

```powershell
cmake --build .\nodsdk-scghost\build\niltomap --config Release --target MAPtoNIL MAPtoNSD MAPtoNPD
```

For the existing SDK-root build, update its conversion executables as well:

```powershell
cmake --build .\nodsdk-scghost\build\viewer --config Release --target MAPtoNIL MAPtoNSD MAPtoNPD
```

Both build surfaces were updated during implementation so editor discovery
would not select stale conversion tools.

Regression sources and their CMake configuration are local-only and excluded
from commits. Production builds do not include these regression targets. If the local
sources are available, configure their separate build, supplying the existing
GLM package settings if they are outside CMake's search path:

```powershell
cmake -S .\nodsdk-scghost\src\tools\tests -B .\nodsdk-scghost\build\local-regressions
cmake --build .\nodsdk-scghost\build\local-regressions --config Release
```

Use a Python environment with Unicorn already available. Set `PYTHONPATH` to
the existing native-test dependency directory if it is not installed in that
environment. Substitute the actual original asset root:

```powershell
$env:NOD_MOPP_TEST_RUNTIME = 'C:\SCGhost\game_files'
$env:NOD_NIL_TEST_ASSETS = $env:NOD_MOPP_TEST_RUNTIME
$env:NOD_WORLD_TEST_MAP = (Resolve-Path '.\scghost-mapping\nod\maps\test_map_aknavj.map').Path

ctest --test-dir .\nodsdk-scghost\build\local-regressions -C Release -R '^(WorldGeometry|WorldRender|WorldSector|WorldImage|MAPtoNPD|MAPtoNSD|MAPtoNIL|WorldNative|LightingNative|ExtendedLightingNative|PhysicsNative|SolverNative)$' --output-on-failure
```

Missing native runtime/dependencies can cause native tests to skip or fail;
a skipped oracle is not evidence of native compatibility. `WorldImage` uses
`world_sector_tests --world-tests`; there is no separate `world_image_tests`
build target.

Direct compilation example, with pre-existing writable workspace/output
directories and read-only original assets:

```powershell
.\nodsdk-scghost\build\niltomap\Release\MAPtoNIL.exe --world-only --workspace .\scghost-mapping\nod --asset-root 'C:\SCGhost\game_files' .\scghost-mapping\nod\maps\test_map_aknavj.map .\scghost-mapping\nod\compiled\Levels\test_map_aknavj.nil
```

For the editor, build the normal Radiant project as Release/Win32 into a
preview output directory. Save and close a running editor before replacing
the installed executable, then compare SHA256 of the built and installed
copies. The initial direct-point implementation's installed executable hash was:

```text
10C22FF7A4BE937FF1C9911B6E2A25E8AD9ED10C0ED1944AF4797D96742574FE
```

This is a historical installation check, not a required hash for future
rebuilds. No MAP edit or original-asset replacement is part of installation.

## 11. Troubleshooting and remaining work

| Symptom | Check or action |
|---------|-----------------|
| No `nod_static_light` in the browser | Use the SCGhost profile and current executable/definitions; restart or reload |
| Definition reload error | The fixed-size class or required definition marker is absent |
| World still uses white baseline | Confirm an authored light exists and conversion uses the updated SDK tools |
| NPC still has no light | Confirm the new NIL contains static lights and the game loaded that output, not an older copy |
| Shadowed surfaces are black | Expected without ambient/GI; enable explicit ambient or adjust actual lights |
| Coarse gradients or shadows | Reduce spacing within the triangle and ray budgets |
| 4096-triangle or vertex-limit error | Increase spacing or simplify world geometry |
| Shadow ray budget exhausted | Increase spacing, reduce lights or simplify occluders |
| Unsupported light property | Remove the unsupported key; only the documented five keys are accepted |
| Conversion fails on navigation/placement | Resolve the reported NPD/NSD authoring error separately; lighting does not bypass it |
| NPC receives light through a wall | Known single-sector native character-light limitation |

Known limitations and follow-up areas:

- Point lights, one directional sun/moon and explicit ambient are supported.
  There are no authored spots, bounce, light styles or automatic light placement.
- No model/entity shadow occluders and no baked character-shadow visibility.
- One world sector; no verified per-sector visibility-aware light assignment.
- Vertex-sampled shadows, not per-texel shadows or a texture lightmap atlas.
- Collision is refined along with rendering, increasing collision data cost.
- Fixed shadow bias and numerical tolerances may need additional validation
  for unusually small or large coordinate scales.
- Linear channel accumulation/clamping is compiler policy; complete
  material/display brightness and gamma behavior is not established here.
- Header-to-object ambient scaling/packing is verified. Remaining native flags,
  other terrain/sky lighting paths and rendered gameplay validation remain unresolved.

Any future change to these contracts should extend the native and publication
regressions, preserve no-light compatibility intentionally, and document any
new author-facing units rather than silently changing existing behavior.

Related references:
[native format contract](FORMAT_REFERENCE.md#native-lighting-contract),
[compiler authoring workflow](../src/tools/README.md#cpu-direct-light-baking),
and [SCGhost editor profile](../../GtkRadiant/gamepacks/SCGhostPack/README.md#static-lighting).
