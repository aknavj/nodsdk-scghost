# NODscene: source-preserving scene bridge

This is the first milestone toward a bidirectional NOD scene compiler, not a
complete MAP-to-NIL geometry compiler. `NODscene` is C++17 and uses the existing
shared NSD/NPD parsers for tagged entities, model overrides, waypoints, and
adjacency, plus shared NOD/NMB parsers for native model resolution. NIL geometry
and legacy OBJ/material conversion retain their C99 helpers. `NILtoMAP` remains C99.
The compiler still patches archived source bytes rather than reconstructing
opaque data through parser serializers. Legacy MAP/sidecar pairs remain supported.

Sources are separated by responsibility: `nil2map/` contains export workflows,
`map2nil/` contains MAP validation and restricted native compilation, and `common/`
contains shared formats and utilities. The existing `NODscene` command dispatches
to those modules without changing its CLI or sidecar contract.

The target compiler will author new levels as well as modify imported ones,
building NMB bundles and linked NIL/NSD/NPD outputs from MAP plus assets.
An imported source sidecar should preserve undecoded data, not be mandatory
for new maps. This remains a future capability: standalone NOD/NOC-to-NMB v2
compilation is implemented by `NODtoNMB`, but automatic dependency collection
and level-compiler integration are not. NIL/NSD serializers copy raw data rather than
constructing new levels. Runtime collision, sectors, and gameplay behavior
still need format research and game validation.

The proof-of-concept order is converter fidelity first, then a native NOD-to-NMB
bundle compiler, then the remaining native level compilation/runtime work.
The standalone bundle compiler preserves verified model names, geometry,
materials and supported model attributes and rejects unsupported layouts. Parsing
and per-member geometry equivalence are necessary checks, not proof of runtime
compatibility. All generated bundles must be written to a separate workspace;
original game assets remain read-only. Editor widget polish is not a prerequisite
for this work.

## Build and use

Build `src/tools` as described in [README](README.md). This builds `NILtoMAP`,
`NODscene` and `NODtoNMB`. Root builds also include all three tools.

```text
NODscene export level.nil level.nsd level.npd scene.map scene.nodsrc
NODscene export-properties level.nil level.nsd level.npd scene.map scene.nodsrc
NODscene preview scene.map scene.nodsrc "extracted-game-root" level.nmb "editor-game-root" preview.map preview.nodsrc
NODscene preview-obj scene.map scene.nodsrc "extracted-game-root" level.nmb "editor-game-root" legacy.map legacy.nodsrc
NODscene compile preview.map preview.nodsrc rebuilt.nil rebuilt.nsd rebuilt.npd
NODscene compile scene.map scene.nodsrc rebuilt.nil rebuilt.nsd rebuilt.npd
```

Use matching source files from the same extracted level/build. Keep the MAP
and its `.nodsrc` source sidecar together; the sidecar is an explicit compiler
argument, so saving the MAP under a new name is allowed.

The version-1 sidecar contains the complete original NIL, NSD, and NPD bytes,
with size fields and CRC32 integrity checks. It is not a portable asset pack.
It contains the user's game data and must not be committed or distributed.
The MAP contains the corresponding source checksums and schema version.
Existing preview-enriched scenes use version 2: the same three original source
blocks, followed by a length/CRC32-protected MAP-format asset-binding table.
Legacy source exports use version 4: brush-free scenes with optional model bindings.
Legacy native previews use version 5: the same archive layout with native NOD/NMB
paths and an additional immutable `_frame` in each binding.
Versions 6/7 add computed region bounds to source/OBJ and native previews.
These retain the version-4/5 archive layouts and add derived, read-only region
bounds to the MAP baseline. This versioning prevents removing or changing bounds
metadata while keeping old MAP/sidecar pairs compilable without new properties.
New exports and OBJ previews use version 8; native previews use version 9.
These archive generated OBJ fallback brushes in the binding table and emit them
as editable `func_group` entities. Native NOD/NMB aliases remain model entities.
Versions 3-9 always append the binding length/checksum header; a zero length
and zero CRC indicate a scene without asset bindings. Version 3 selects stable
plane triples for tiny geometry. The compiler continues to accept versions
1-7 and regenerates their original brush/model representation, so existing MAP/sidecar
pairs remain compilable. Compilation needs only the MAP
and sidecar, not the game asset catalog or generated preview files. Do not mix
an old sidecar with a newly regenerated MAP.

Export validates inputs and constructs both outputs before opening output
files. Compilation validates the entire MAP against a regenerated baseline
before opening outputs. Invalid/unsupported edits leave existing outputs
untouched. Filesystem write failures can leave partial outputs; multi-file
publication is not transactional. Always compile to a separate staging
directory and keep original game assets backed up.

## Custom MAP schema

### Opt-in native property editing and extraction

`export-properties` produces schema 14 source maps; enrichment retains this
property contract (schema 15 for native previews, 14 for OBJ previews). Ordinary
`export` and existing schemas 1-13 retain their original behavior.

Verified counted NSD arrays expose the last local occurrence of each property
as `nod_prop_ID`. Known storage supports printable ASCII string32 (edits up to
31 bytes plus terminator), finite binary32 scalar/vector3, and raw unsigned
32-bit integer bits. Numeric edits touch only 4/12 bytes; unused payload bytes,
earlier duplicates, counts, record order, and all opaque data are preserved.
No-edit compilation is byte-identical. IDs 1/2/3 use existing transform/sector
keys rather than duplicate editable aliases. Unknown storage, nonfinite values,
and invalid strings appear as read-only `nod_raw_ID` hexadecimal payloads.
Unverified legacy arrays are explicitly diagnosed and not made editable.

`basedon` (0), native `type` (20), raw values, and source identities are protected.
Model (4), renderflags (12), script (15), MODELSCALE (143), and MODELOFFSET (175) can be edited
in source maps only: re-enrich afterward to rebuild immutable preview bindings.
Brush-linked parent properties remain protected against stale baked geometry.
No field addition/deletion, implicit inherited overrides, or template/type changes
are supported. Inherited-only values must be edited in NOT. This validates storage
and preservation, not every gameplay range, reference, or script handler.

```
NODscene entitydefs types.def --assets-root extracted-root --inventory observed.json level.nsd [more.nsd ...]
```

The generated per-type `.def` lists observed property names, storage, local versus
inherited availability, instance counts, and edit restrictions. Optional JSON
includes every observed placement, every counted local modifier (including
duplicates and raw bytes), effective values, archive/template provenance, and
explicit resolution errors. The full NOT context includes each NSD's declared
archives. Without `--assets-root`, local extraction remains available and inherited
values are explicitly unavailable. This is an observed catalog, not an exhaustive
engine registry or a set of invented per-type defaults. Inventories contain local
game data; do not commit or distribute them.

Source exports have a `worldspawn` and metadata points. Verified NSD environment
layouts expose the optional environment contract described below. `preview`
represents verified NOD/NMB placements as native entity model aliases. Generated
OBJ fallbacks, including NIL static geometry, become editable brush groups.
No CSG, BSP, collision, lighting, UV, or portal recompilation is performed.
Compilation retains the original NIL bytes.

### Manual native controls and imported script editing

The primary workflow places actual NOT templates and attaches a verified native
handler in GtkRadiant's **N > Selection > Native handler** selector.
`NODTemplates` publishes the SDK-owned behavior registry: typed defaults,
field labels, editability, numeric limits, resource kinds, frame initialization
and trigger connection layouts. GtkRadiant consumes these descriptors rather
than reproducing handler rules. Template compatibility uses resolved NOT
properties, including placement overrides, not template-name allowlists.
Interactive modeled type-2 props (thingflags bit 4) support `cTriggerOnAction`
and `cButton`; nonmodeled/noncolliding type-2 helpers support `cSoundStinger`,
`cAIFightPath` and `cTriggerRelay`. Modeled colliding type-2 props support
the translation-only `cDoorMove` workflow. Existing wall/floor/Protoss console
variants and compatible templates in additional archives use the same path.
`cButton` has 18 parameters and no animation frames; `cDoorMove` has 11
parameters and two translation frames.
Native type-3 regions support `cRegionTrigger` and the verified
`cObjectiveUpdate` subset: initial enable, deployed mission objective keys, and
the first linked output. Both use `geometry=region` metadata and the complete
inline region model/tail. Objective keys are syntax-checked, not resolved against
the active deployed mission. Advanced objective parameters remain read-only for
new authoring; existing source-backed bodies retain their original payloads.
`cCutscenePlay` supports native regions and nonmodeled/noncolliding type-2
helpers. Owner-specific catalog schemas prevent inline region geometry from
being serialized on helpers. The verified 20-parameter subset exposes NCS name,
one-shot and the finish/stop output (slot 14, fixed `p0=0`). Video/preload/startup,
actor cleanup and timeline-event edits are not exposed. Resource-name validation
is shared by ordinary authoring and edited nonvideo source-backed bodies; it
checks syntax, not deployment or mission compatibility.
`cTimerTrigger` supports nonmodeled/noncolliding type-2 helpers with ten
parameters: two alternating float intervals and eight output references.
It starts automatically, firing all outputs with `p0=0` on each expiry, and
has no trigger input. Intervals are 0.001..599.999 seconds, matching nonzero
millisecond scheduling and the native ten-minute rejection window. Edits use
the same bounds; unchanged imported bodies are preserved. New additions cannot
exceed 128 combined weighted startup timers from timer triggers, cycling damage,
sound loops and gameplay controls, including imported owners; the native pool
is shared with other script timers, so leave runtime headroom.
`cRegionSound` adds native type-3 sound regions: reverb `-1` (unchanged) or 0..25,
plus an optional deployed ambient cue. Its `[1,7]` body has no frames or trigger
input/output. Primary-player entry applies settings; a half-second exit check
restores world defaults only when no sound region remains occupied. It is not
a restoration stack, and a blank world ambient cue does not stop an existing
loop. Shared validation covers fresh/edited imported parameters; no-edit
roundtrips retain original payloads, with the all-ones sentinel displayed as -1.
`cRegionSave` adds native type-3 checkpoint regions, kinds `[1,1]`: marker
`-1` (skip registration) or `0..15`, plus initial enable `0/1` (default 1).
There are no fresh frames or outputs. Trigger input only enables; entry by a
savable controlled player consumes one attempt, requests native checkpoint
capture, shows the save hint and advances the current marker. A rejected request
still consumes it; the hint is not a persistence acknowledgement. The consumed
latch is saved/restored. Marker registration occurs even while disabled; ID 0
requests the initial marker. Duplicate registrations overwrite. Typed imported
edits share validation; unchanged legacy values and two-frame bodies are retained.
Fresh zero-frame checkpoints use current-player respawn placement. Disk/profile
publication and live save/load depend on the original runtime/mission.
`cRegionStealth` adds a type-3 `[2,1]` body: unclamped finite location-stealth
value (default 0.5) and initially disabled `0/1` (default 0), with no frames
or outputs. Each trigger toggles; controlled-player entry writes the value;
exit resets zero regardless of enabled state or other occupied regions.
Toggling does not refresh occupants. Typed imported edits share validation;
unchanged legacy flags remain exact. Raw field writes and native region
dispatch are verified, but the tested visibility query is unchanged.
Do not treat this as a proven AI/HUD effect or invisibility percentage.
`cRegionKillZerg` adds a type-3 `[3,3,3,3]` body with zero frames:
four region-local actor exemptions, not linked outputs. Nonexempt actor
kinds 0/7 with actor-type bit 0x8 request original native death on entry;
native guards still apply. No trigger input, timer, one-shot or exit action
exists. Existing fresh/imported reference validation applies; unchanged
shipped -1 sentinels and opaque region geometry remain exact. Original
dispatch, filtering and death-state writes are verified with presentation/AI
boundaries; live initial-overlap tracking and gameplay remain unverified.
`cRegionDamage` adds a zero-frame type-3 body, kinds
`[2,1,2,2,1,1,1,1,3,3,3,3]`. Primary entry starts immediate/0.25-second
native damage ticks while enabled and occupied. IDs 0/2/3 toggle, ignoring
float data; rapid re-enable can duplicate queued tick chains. Optional phases
require both durations nonzero and add 0.1 seconds each, with edited/new
durations bounded to 0..599.899. Cyclic owners share the startup timer budget
with timer triggers, sound loops and gameplay controls. New kill modifier/damage type are fixed at zero; unchanged
imported legacy values roundtrip and remain read-only. Non-primary actor-kill
mode ignores the enable gate; container mode uses four references, not action
outputs. Native health subtraction, callbacks, phase limits and exact
source-backed triples pass; scheduling, lookup, AI/presentation and container
opening are fixture boundaries. Live tracking/HUD/gameplay remain unverified.
`cFogSet` adds a zero-frame body, kinds `[1,1,1,1,1,2,2,2,1]`, on native
type-3 regions or unmodeled/noncolliding type-2 helpers. Any kind-7 player
entry captures/applies fog; exit mode 1 restores, mode 2 keeps. Triggers
toggle a snapshot regardless of ID/data. SDK-owned scalar limits cover RGB,
fog mode, exit mode, transition and smoke; fresh and relevant imported edits
also require linear start/end ordering in binary32. Unchanged legacy values
remain exact. Native snapshot/interpolation/effect-manager writes, metadata,
Undo/Redo and exact triples pass. Fog controllers share global state rather
than a restoration stack, and pending smoke timers survive fog restoration.
The saved active flag does not serialize the captured snapshot. Lookup,
scheduling and GPU handoff are fixture boundaries; live rendered results
remain unverified.
`cSoundLoop` adds a zero-frame type-2 helper body, kinds `[7,2,2,2]`.
A deployed cue auto-starts through a 0.1-second timer. Legacy floats 5/10/1
are fixed/read-only because the native callbacks do not use them. Triggers
ignore ID/data and start only while the cached handle is zero; stopping a
nonzero handle does not clear it, so subsequent triggers cannot restart.
EndScene stops/clears it. Loop startup timers share the 128-slot guard with
timer triggers, cycling damage and gameplay controls. Imported cue edits are validated; unchanged
blank cues and legacy floats remain exact. Native callback/wrapper/normal
mixer stop flags, editor metadata/Undo/Redo and exact triples pass.
Mixer playback/lookup and scheduling remain fixture boundaries; live audio
and spatialization remain unverified.
Fourteen additional controls bring the registry to 31 workflows:
`cParticleToggle`, `cEffectAdd`, `cObjectExplosion`, `cRain`, `cObjectMove`,
`cObjectRotate`, `cObjectSetCollision`, `cObjectSetColor`, `cObjectRemove`,
`cPhysicsActivate`, `cHelpText`, `cPopupConversation`, `cMissionNext`,
`cLevelChange`. The [tools guide](README.md) describes owner compatibility
and trigger semantics. Native resources carry explicit optionality through
`resource=native-resource` and `allow-empty=0/1`, which the editor must retain.
Fresh/edited bodies share fixed-field, resource and cross-field checks;
unchanged legacy values/frames remain exact. Model translation/rotation publish
tracker properties. Startup accounting is weighted: collision uses one slot
plus another when initially hidden; shader setup, automatic translation,
automatic explosions and timed initially enabled particles each use one.
Popup's sole output is its last-key removal notification; removal references
are not outputs. Presentation/physics/UI/mission handoffs remain native fixture
boundaries; exact callbacks/roundtrips do not establish live gameplay.

Other native handlers are not assumed compatible merely because property 15
can name them.

Catalog XML version 2 adds root `behavior` schemas and per-template `spawnable`
capabilities. Use matching SDK/editor builds; version-1 catalogs remain readable
for ordinary template metadata but cannot configure new behavior schemas.
Archive ordering retains the native first-loaded-wins rule, including Global.

NOT placements retain their provenance/modifier ordering. An authored
`nod_prop_15` requires the complete `parameters-v1` parameter/frame group;
counts, kinds, frame count, owner compatibility and references are validated
before publication. `targetname` is authoring metadata, not a NOT modifier.
Generic placements precede preset records; aliases are resolved over both
sets before writing native indices. Cycles, dangling references and action
graphs over 64 transitive actions are rejected.

Source-backed additions use the same placement compiler:

```text
NODscene compile input.map input.nodsrc output.nil output.nsd output.npd workspace assets-root
```

The optional workspace/asset arguments are required for new NOT placements.
Their ordered archive context must match the imported NSD; existing records,
NIL geometry and unrelated navigation/environment data remain preserved.
The original shorter command remains valid for existing supported edits.
In Radiant, Ctrl+K links consoles/buttons to cues, relays or model movers and appends relay actions;
relay-to-AI wiring adds a start/stop pulse rather than an autonomous wave.
Reset/removal is atomic and Undo-backed, does not alter shared NOT archives,
and does not replace the original entity/model.

Model movers toggle between two stops; they do not implement a generic
floor-selection protocol. Stop 0 must match the owner origin, both native
rotations must match its orientation, and stops must differ by at least 0.001
world units. Parameter 1 is a strictly positive speed in world units/second,
not a duration. The compiler appends local `MOVETYPE=2` and `PHYSICSFLAGS=1024`
without changing NOT files, and rejects conflicting explicit overrides.
Portal/pivot/jitter/notification settings remain fixed to the verified
translation-only defaults. Wall buttons expose eight target references and
0/1 one-shot/initially-locked settings and an optional shader reference.
The original door-switch model retains its shader default; other models start
without a shader animation. Missing shaders do not prevent native dispatch.
Other button settings remain fixed.
Native button activation delays dispatch by 0.5 seconds and prevents reuse for
1.5 seconds. Actual native callback, two-direction tracker interpolation and
arrival dispatch are exercised in local regression tests; full Havok collision
and player riding still require in-game verification.

New MAPs use `MAPtoNSD`; imported source-backed MAPs use `NODscene compile` with
their matching `.nodsrc`. Both accept these strictly validated point presets:

| Class | Required keys | Optional keys |
|---|---|---|
| `nod_console` | `target` | `nod_console_template`, `targetname`, `origin`, `angles` |
| `nod_sound_cue` | `targetname`, `nod_cue` | `origin`, `angles` |
| `nod_enemy_spawn` | `targetname`, `nod_enemy`, `nod_destination` | `nod_max_spawns`, `origin`, `angles` |
| `nod_trigger_relay` | `targetname`, `nod_relay_count`, indexed `nod_relay_target_N`/`nod_relay_delay_N` | `origin`, `angles` |

Each entity also requires its exact `classname`; no `nod_authoring` or template
registration is required for presets. `origin` and editor-degree `angles` default
to `0 0 0`; angles use the existing editor/native conversion. Native console front
is local +Y and vertical is world Z. `nod_destination` is a finite world XYZ vector.
`nod_enemy` names an exact template in the active NOT archives. Candidates must
resolve to native actor type 0, movement type 1, a model and actor AI; abstract
helpers and actor-like props without AI are rejected. Radiant's **Choose NOT
template...** picker lists these candidates; new spawners require an explicit
choice. Compatibility does not guarantee every original AI has suitable mission
resources/navigation; deploy and test its dependencies in game.
`nod_max_spawns` is a
canonical decimal 1..25, default 25. Target names are case-sensitive 1..63-byte ASCII
letters/digits/`_-.:`. `nsd:` is reserved for canonical decimal
original byte-offset identities of imported objects. Cue names are 1..31 ASCII
letters/digits/`_-.`, with no leading/trailing dot.

Presets serialize the original `cTriggerOnAction`, `cSoundStinger`, and
`cAIFightPath`/`cTriggerRelay` schemas, using `hackConsole_wall_activate`,
`script_object`, and `script_target`. `nod_console_template` optionally selects
another compatible interactive model. Catalog-backed presets require NOT assets
at compile time; old console/sound-only presets remain asset-independent.
Spawn aliases address the hidden pulse relay,
not the AI script; the relay sends start immediately and stop after 0.25 seconds.
Consoles are reusable; runtime interaction timing and the lifetime cap remain
native behavior. Sounds do not play on scene start or loop; they request the cue
via the original triggered 3D stinger path. This does not deploy audio banks or
force score playback. New records use sector 0. Hidden relays count toward the
1024 final-object limit. Targets become zero-based serialized object indices,
never runtime GUIDs. Duplicate/unknown keys, invalid numbers, aliases, missing
fields and dangling references fail before output files are opened.

`common/nod_script.h` exposes `nod::script::authoring_class(Entity)`
and `compile(Map, first_index, existing_targets, records, error, templates)`. Existing targets
map identifiers to indices below `first_index`. Only the four preset classes are
gathered; worldspawn and unrelated records are ignored. Complete native object
records are appended to `records` only after successful validation. Imported
compilation needs no asset catalog or reconstruction of original objects.

`NODScript` is a version-1 text-source compiler. `--workspace DIR --asset-root DIR`
and optional `--archive NAME` arguments enable the same catalog validation during
expansion or `--check`. Without assets, the CLI explicitly reports that template checks
are deferred to MAPtoNSD/NODscene.

It preserves a supplied MAP's
geometry and native identities, removes its build-only `nod_script_file`
worldspawn key, and appends native control declarations. Its expanded MAP is
consumed by `MAPtoNSD` or, with the matching original sidecar, `NODscene compile`.
`--check SOURCE.nods INPUT.map` validates without writing; normal compilation
requires `--workspace DIR SOURCE.nods INPUT.map OUTPUT.map`, with output inside
the workspace and distinct from both inputs. Writes use the existing atomic
publication helper. The source limit is 1 MiB; input MAP limit is 64 MiB.

Syntax begins with `nodscript 1;` and supports named `console`, `sound`, `enemy`
and `sequence` blocks. Settings and `trigger TARGET;`/`wait SECONDS;` statements
end with semicolons; vectors use `(X, Y, Z)`. Waits accumulate relative to sequence
activation. Relay records have the verified 36 typed parameters: twelve
GUID/delay/text triplets, with unused slots empty and supported receivers sent
`"0 0 0 0"`. Spawn targets resolve to their hidden pulse relay, never the AI.
Cycles, console receivers, unverified imported receiver classes and more than
twelve transitive trigger actions are rejected. Delays are finite nonnegative
float32 seconds. No conditionals, VM, arbitrary code or looping score mode is
introduced. See the GtkRadiant game-pack guide for a complete source example.

The editor selects a source file in its export dialog, persists the reference
in the MAP, and snapshots it before expansion. Scripts therefore do not add a
mutable external dependency between NIL, NSD and NPD export stages. Script errors
carry source file/line/column diagnostics; failed expansion leaves runtime
outputs untouched. Final native layout and object-count constraints remain the
responsibility of the scene compiler.

Verified imported script bodies expose `nod_script_authoring=parameters-v1`,
immutable `nod_script_param_count`, `nod_script_kind_N`, and `nod_frame_count`.
Editable `nod_script_param_N` values use native kinds: 1 unsigned 32-bit bits,
2 finite binary32 float, 3 symbolic object reference or `-1`/`-2` sentinel, and
7 printable ASCII up to 255 bytes. GUID-kind references are exported as
`nsd:ORIGINAL_OFFSET` and may reference added preset aliases. Existing frame keys
`nod_frame_origin_N`, `nod_frame_rotation_N`, `nod_frame_sector_N` address verified
28-byte frames; rotations are native Euler values, not editor angles. Edited
frame sectors currently support sector 0 only; unchanged source sectors are
preserved. Script class changes are rejected when the verified schema is exposed.
Numeric edits patch only their payload bytes; string replacements are spliced
in descending source-offset order after fixed-size edits. Unknown and opaque
sections retain their exact bytes.

Worldspawn uses `nod_environment_authoring=nsd-environment-v1` and the same
music/ambient/sky/fog/far-clip/reverb/sky-unmasked keys as `MAPtoNSD`. Removed
fields with the marker present clear/default that setting. Untouched no-fog
imports preserve undecoded fog words; unchanged filename fields preserve their
original bytes, including original basename-NUL-extension encodings. Unexposed
flags/volume words and opaque world arrays remain unchanged. Environment and
append support require a verified last-object boundary, zero source sound-array
count, and valid opaque world-array strides (60/96/96 bytes). Unsupported layouts
remain importable and byte-preserving without exposing unsafe environment fields;
appending to them fails explicitly.

Old schema 14/15 (and earlier) MAPs may omit the entire new script/frame or
environment group and compile unchanged. Partial script groups, changed kinds,
frame/parameter counts, unknown keys, existing NSD deletion, geometry, identity,
class and source integrity changes remain rejected.

Imported v2 NPD nodes/links can be added, deleted and connected when the source
graph has normal flags, unassociated nodes and sector 0. Existing waypoint IDs
remain immutable. Imported link endpoints retain original **record-index**
semantics; newly authored `npd-world-v1` links address **waypoint IDs**. Rebuilds
translate both to final indices, preserve unique IDs and the next-ID watermark,
and recompute component/adjacency metadata. Dangling endpoints, duplicate IDs or
directed links, self-links and excessive adjacency fail. Topology with non-normal
associations, sectors or link/node flags is rejected explicitly; no-edit imports
and ordinary origin/property edits retain their original opaque bytes.

| Class | Representation | Editable fields |
|---|---|---|
| `nod_nsd_type_<encoded native type>` | Native NSD entity; a model is visualization, not a replacement entity | `origin`, existing `nod_sector`, and `angles` for model-backed points or `nod_rotation` for other points; linked-brush placements remain read-only |
| `nod_scene` | Schema and source binding | None |
| `nod_nil_sector` | Sector center, flags, portal count | None |
| `nod_nsd_entity` | Tag-validated native scene entity | `origin`, `nod_rotation`, existing `nod_sector` |
| Legacy `misc_model` | Editor-only alias for one resolved native NSD entity | `origin`, `angles`, existing `nod_sector` |
| `func_group`, `nod_role=nil_geometry` | NIL triangle brushes, 0.01 units thick | MAP geometry; native compilation requires unchanged geometry/metadata |
| `func_group`, `nod_id=nsd:<offset>` | Generated OBJ fallback, placement baked into brushes | MAP geometry; native compilation requires unchanged geometry/metadata |
| `func_group`, `nod_id=nsdgeom:<offset>` | Version-10+ geometry linked to a native entity by `nod_parent` | MAP geometry; native compilation requires unchanged geometry and links |
| Legacy `misc_model`, `nod_id=nil:world` | Old NIL static render-mesh preview | None, including transforms |
| `nod_npd_waypoint` | Native waypoint record | `origin`, `nod_flags`, `nod_color`, `nod_extra` |
| `nod_npd_link` | Native adjacency record | `nod_from`, `nod_to`, `nod_cost` |

Every non-world entity has an immutable `nod_id`: a source NSD byte offset,
NIL sector index, NPD record index, adjacency record index, or `nil:world`.
Entity/key order, whitespace, comments, and equivalent binary32 numeric
formatting do not affect validation. Version-4/5 scenes reject added brushes.
For legacy version-1/2/3 scenes, brush/face order and cyclic face-point
permutations are accepted; optional zero-valued classic MAP face flags may be omitted.

Other legacy geometric rewrites, including alternate plane points describing the same
plane, may be rejected conservatively. A real GtkRadiant open/save cycle is a
separate validation gate; synthetic text normalization tests are not a substitute.
Brush primitives are supported for archived fallback geometry; patches are not supported.

The compiler rejects entity creation/deletion, duplicate identities/keys,
unknown keys, edits to read-only data, material/texture/geometry changes, invalid
numbers, invalid sector assignments, and graph endpoints outside the node array.
It never silently discards unsupported MAP edits.

### NSD entities and asset references

Source exports use schema/sidecar version **10**. New diffuse and native previews
use **12** and **13**, with eight-byte magic `NODSRC12`/`NODSRC13`. Versions 1-11
remain compilable with their original class, rotation, and unscaled-model contracts.

The source's native `nod_type` is retained, including props, characters,
triggers, regions, and nonvisual types. There is no Quake gameplay-class mapping.
Every tag-validated native type gets a deterministic `nod_nsd_type_` editor class.
ASCII letters/digits retain their spelling and case; every other byte, including
underscore, becomes `_HH` (uppercase hexadecimal). This mapping is injective and
does not confuse different native type names. The compiler validates the class
and exact `nod_type` against the original NSD, rather than guessing behavior from
a model name.

`NODscene entitydefs output.def input.nsd [input.nsd ...]` generates a sorted,
deduplicated palette of observed native types. The supplied
`radiant/nod_types.def` and SCGhost gamepack catalog cover 193 types observed in
the supplied levels, not a verified exhaustive engine registry. Radiant's `.def`
loader reads this file alongside `entities.def`. Each entry is a prototype:
unknown record layouts, defaults, required references, and gameplay parameters
are not fabricated. The generic `nod_nsd_entity` remains available for legacy
and otherwise unclassified points.

Property lines retain native names, storage types and edit-policy markers;
shared restrictions appear once per entry.

Native NOD/NMB previews now remain on these native entities, using the existing
model renderer, member selection, bounds, and editor-angle adapter. Unresolved
entities keep their native class and selection marker or inline region bounds.
For generated OBJ fallbacks, the native entity retains `nod_id=nsd:<offset>` and
links through immutable `nod_geometry` to a separate `func_group` with
`nod_id=nsdgeom:<offset>` and the reciprocal `nod_parent`. Brush vertices remain
baked into world space. Their parent's placement is currently read-only, matching
the previous brush fallback restriction; it cannot move independently and leave
the preview silently stale. Re-enrichment from editable source placements rebuilds
the linked geometry.

Versions 12/13 archive immutable `nod_modelscale` and `nod_modeloffset` on each
preview binding and native entity. The NIL world binding is always identity.
Model scale is the uniform ID 143 value applied after model creation; an earlier
scale is ignored and a newly created model starts at one. ID 174 is LOD scale,
not geometric scale. ID 175 contributes a world-space offset only when renderflags
(ID 12) contain bit `0x2000`:

```
preview_vertex = entity_origin + enabled_offset + native_rotation * (model_scale * model_vertex)
```

Entity `origin` remains the native NSD position, not the displaced mesh origin.
Radiant applies these private fields to model rendering, bounds, and selection;
OBJ fallback brushes bake the same transform without modifying cached OBJ data.
Inline region bounds remain independent of external model transforms. No name-based
`_offset` recentering or Quake `modelscale` key is used. Negative scale is retained;
zero scale becomes a diagnosed `zero-model-scale` point with its reference preserved,
not degenerate brushes. Nonfinite active values and enabled offsets without an
explicit ID 175 fail rather than guessing uninitialized engine state. Private
transform changes are rejected by native compilation before writing output.
Re-enrich older previews to opt into the corrected transform contract.

This is the entity-format foundation for future from-scratch compilation, not
that compiler itself. Creating/deleting entities, changing native types,
compiling edited NIL geometry, rebuilding NPD topology, and validating game
execution still require native serializers/schemas. Undecoded data is retained
byte-for-byte in the source sidecar. Unsupported edits fail before output files
are opened.
`origin` and `nod_rotation` expose tagged position/rotation floats in the native
XYZ/component order. A `nod_sector` field is exposed only for tag-3 records;
tag-15 region class names are read-only metadata.

The NSD reader uses the repository's tag-validation pattern (32-byte type,
tag 1 at +32, tag 2 at +68), not a fully decoded variable-length record table.
It additionally requires zero padding in the type-name slot. Real level data
confirmed that omitting this check misidentifies property-string suffixes as
entities, including a spurious non-finite transform.
It warns if the number of exposed transforms differs from the header count at
offset 80. All original bytes remain in the sidecar regardless of exposure.

Direct tag-4 model references in the first four optional property slots are
retained in `nod_model` when nonempty. Preview enrichment also distinguishes an
explicit empty tag-4 override from an absent override: an empty override does
not inherit a template model.

### Verified asset previews

`preview` first validates and applies supported MAP edits, then creates a new
source snapshot and resolves model references. Keep its output MAP and sidecar
together. Re-enrichment of an already enriched scene is supported.

The default `preview` command targets the fork's **NOD Authoring**
(`scghost.game`) profile. Props use VFS-relative native NOD/NMB paths,
not generated OBJ files. `_frame` is the zero-based **parsed model-entry
index**, not an animation frame or NSD placement index. NOD uses frame 0.
Positions, mesh dimensions, and rotations retain their existing native units
(scale 1); the 32x scale in hand-authored prop examples is not applied.
Model paths, frames, asset/member identities, and checksums are immutable.

A standalone NOD is preferred over a matching level-bundle member only if the
shared parsers confirm identical rest-pose vertex attributes, indices, LOD-0
groups and materials, with no differing baked bundle colors. A same-named but
different NOD is not a safe replacement; the original NMB member and frame
are retained. This verifies editor render data, not animation/game semantics.

Native models bind `textures/nod/<native shader name>` directly. The profile
loads the original NSA material definitions and loose images from the mounted
asset tree. No converted prop meshes or images are generated.
NIL static geometry alone needs intermediate OBJ/MTL, with the same native material
namespace and no generated diffuse images. The MAP uses brushes, not that OBJ.
The material loader, not the exporter, reports missing native images.

The intermediate NIL world model uses native XYZ positions and original
nondegenerate strip/list triangles without sector-center offsets.
Its identity is `nil:world`; intermediates use
`models/nodpreview/nil-<NIL CRC32>-d<MTL CRC32>.obj`.
Earlier neutral preview paths remain compilable.
Each triangle becomes a five-plane prism extruded 0.01 native units behind its
front surface. Front faces retain the source material; other faces use caulk.
Generated OBJ entity fallbacks receive native rotation/translation before
extrusion. Their MAP groups have no `model` or `_frame` keys, and compilation
does not require intermediate OBJ files. Brush-primitive matrices preserve
original normalized mesh UVs, including mirrored, sheared, and constant mappings.
The generated OBJ V compensation is reversed before fitting brush UVs.
This is editable render geometry, not reconstructed CSG/collision or a recreation
of native multipass material effects. Versions 8/9 introduced archived fallback
geometry; versions 10/11 additionally preserve native entity classes and linked
geometry. Existing classic archives remain compilable; new archives retain UV matrices.
Plane-point order is retained for brush primitives because cyclic reordering can
change the binary32 normal and thus its UV basis. Near-degenerate slivers with
ill-conditioned mappings can exceed binary32 matrix precision; the exporter
reports UV roundoff exceeding 0.001 rather than silently claiming exact recovery.
Current GtkRadiant builds automatically preserve fractional plane points and
skip tiny-brush cleanup for identified NOD groups/point markers. Fallback
brushes use 0.001-unit clipping tolerance; ordinary brushes retain their defaults.
Opening a native brush-primitive map into an empty editor selects that format
automatically, without converting its UVs to classic alignment. Imports into
populated maps and ordinary maps retain the existing format-selection behavior.
Older builds require **Don't clamp plane points** enabled and **Clean tiny
brushes** disabled so loading does not collapse the thin brushes. Thickness is
nominally 0.01, subject to binary32 MAP coordinate precision.
The groups can be edited and saved, but native compilation explicitly rejects
geometry, material, group-placement, and metadata edits before opening outputs.
Untouched brushes compile to the original NIL bytes; no NIL geometry compiler
is implemented yet.
An empty render mesh still has an archived binding with `no-render-triangles`
status, but no model entity or OBJ. Original NIL bytes, including collision,
sectors, portals, and unknown data, remain unchanged.

The extracted game root must contain `Misc/Templates/Global.not` (NOT v1) and
`3D/Models`. It must also contain every archive named in the NSD header, relative
to `Misc/Templates`. The header's archive-name list precedes the object count;
entity identities retain their actual shifted source offsets.

The shared archive database loads Global first and then scene archives in
declared order. Names/files are case-insensitive; extensionless names gain
`.not`, repeated archive requests are loaded once, and nested relative names
are supported. Absolute/traversing references are rejected. Template-name
collisions use **first-loaded wins**, matching the engine hashtable. This does
not change last-applied-wins expansion within a template/placement. Cross-archive
references can target later files because the complete set is indexed first.
Shadowed definitions are diagnosed. A missing/malformed requested archive fails
preview generation rather than using a partial context or replacing an existing
preview output.

Native lookup also indexes optional `3D/Terrains`, preserving nested VFS paths.
Model selection uses:

1. Expand the complete validated counted NSD modifier array through the shared
   NOT resolver. References expand case-insensitively at their source position;
   later values replace earlier ones, including local duplicates and late
   template references. Select effective modifier 4, preserving its NOT/local
   provenance and any explicit empty value.
2. For legacy placements without a verified counted array only, report a warning
   and retain explicit NSD tag-4 selection followed by NOT fallback. Missing
   dependencies are reported; cycles, invalid references, and expansion beyond
   128 modifiers fail. A local model never bypasses a failed dependency in a
   validated array. Modifier 47 is `DESCRIPTIONID`, not a root-flags field.
3. An exact, case-insensitive member name in the explicitly selected level NMB.
4. Prefer a verified render-equivalent standalone NOD for that member.
5. Otherwise retain the member, or resolve an exact standalone catalog path.

Extensionless references match exact `.nod`/`.nmb` stems. No fuzzy matching,
hardcoded archetype lookup, extension substitution, or automatic search of
other bundles is used. The level bundle argument is a **filename in
`3D/Models`**, not an external path; use `-` to disable member lookup. A reference
to an NMB filename alone can preview a single-member bundle, but cannot infer a
member in a multi-member bundle. The latter remains a point with
`bundle-member-required` status, rather than previewing an entire level bundle.
Missing/ambiguous references remain native point entities and are reported on
stderr and in immutable `nod_preview_status` metadata. They are not guessed.

All supplied NSDs have verified arrays; their existing preview selection is
preserved. The current real-asset set declares no additional archives, so
generated fixtures separately cover scene archive precedence and cross-file
references in both OBJ and native previews. Global property provenance retains
`not:<template>:property4` for archive compatibility; other archives use
`not:<archive>:<template>:property4`. Static model scale/offset application is
shared with the viewer. Animation, skeletal/gameplay handlers, NOB-mounted lookup,
and dynamic engine archive unloading are not implemented.
Effective-property expansion does not replace
the existing placement transform/sector patching contract or simulate gameplay.
The editor's full metadata catalog describes storage, not every engine handler's
gameplay validation. Legacy `nod_script` metadata is retained for archive
compatibility; native modifier 15 is `script`, whereas 161 is `EFFECT1`.

Legacy version-4 through version-9 resolved entities use a single `misc_model` editor alias, retaining the original
`nsd:<offset>` identity and `nod_type`; no proxy entity is added. Immutable fields
record the original reference, NOT/NSD provenance, catalog asset, exact member,
asset CRC32, resolution status, native `model` path, and `_frame`. Editing these fields,
adding scales/remaps, or changing the class is rejected. Compilation
patches the original native record and does not introduce Quake entities or
change native asset references.

### Legacy diffuse-texture previews (`preview-obj`)

The opt-in `preview-obj` command uses the generated-diffuse asset workflow and
its preview-only profile. Versions 8/9 convert generated OBJ meshes into 0.01-unit
brush groups; versions 10/11 retain the NSD entity alongside its linked brush group.
Older sidecars retain model aliases.
The rest of this subsection describes its asset generation, not the default
native workflow.

Shared top-level NSA/NAS archives in `Materials` and `3D/Materials` supply
material definitions. An explicitly selected level NMB also selects the
matching `Materials/<bundle stem>.nsa`, whose definitions override shared ones.
Other numbered level archives are excluded. NSA whitespace, quoted names,
nested blocks, and `;`, `#`, `//`, and block comments are supported.
Only a top-level native `texture` reference is used; when absent, the first
`frame` is a static preview. Editor thumbnails are not substituted for diffuse
images. Without a definition, an exact shader-name DDS lookup is attempted.

Qualified references resolve exactly. Bare DDS names follow the existing
viewer's native search order: Common, Metal, Stone, Wood, Organic, Glass,
Effects, Terrain, Colors, Lights, Shaders, the `Materials` root, then
`3D/Materials`. Custom subdirectories are considered last. Conflicting
definitions at the same priority, differing duplicate DDS files at the same
search priority, missing images, and nonvisual/default materials are reported
explicitly and remain white; no fuzzy or arbitrary filename selection is used.

The dependency-free C decoder exports the top mip of 2D DXT1/DXT3/DXT5 or
16/24/32-bit RGB DDS images as 24-bit, top-origin TGA. Alpha, normal maps,
gloss/environment maps, animated playback, and multipass shaders are not
reproduced. Cube/volume images and unsupported/malformed referenced DDS data
are errors. XPR-only catalogs need their loose DDS files extracted first.

Each OBJ material group gets a separate PicoModel surface, including NIL world
geometry; changing `usemtl` alone would incorrectly texture a whole surface
with its last material. Material names are VFS-relative generated TGA stems,
so the preview profile needs no native NSA shader parser. Texture paths include
the source DDS checksum; model paths additionally include the generated MTL
checksum. Re-enrichment therefore does not overwrite earlier neutral or
different textured previews. Different existing cache contents are rejected.
Compilation still needs only the MAP and native sidecar, not these textures.

For aliases, edit `angles`, not the read-only `nod_rotation`. The conversion
matches the existing viewer's native `Ry * Rx * Rz` convention and GtkRadiant's
`Rz * Ry * Rx` matrix, including its Y/Z/X `angles` key ordering. Unchanged
editor angles preserve the original native rotation bytes; edited angles are
converted back by orientation. This convention has matrix regression tests,
but has **not** been verified against game execution.

The editor root's parent must already exist. Legacy mode creates
`models/nodpreview` and `textures/nodpreview` under that root. Configure it as the
GtkRadiant game project's asset root so the relative `model` paths can load.
Generated OBJ/MTL files contain native XYZ LOD0 geometry and normals, with
diffuse textures resolved from native NSA definitions and loose DDS images.
OBJ V coordinates are negated to compensate for GtkRadiant PicoModel's
inversion; the editor receives the original native UV direction.
NOD vertex types 0-5 and NMB versions 1/2 are
decoded with sequential TBN streams, per-set vertex bases, shared index cursors,
and member isolation. Authored NSA materials, skinning/bone poses, animation,
physics, and gameplay semantics are not converted. These are **unanimated source
meshes**, not verified runtime appearances.

Existing preview cache files are reused only if byte-identical; different
contents are never overwritten. A failed enrichment may leave newly generated
cache files, and output MAP/sidecar publication is not transactional. Generated
meshes contain proprietary game data: do not commit or distribute them.

Inline region meshes, scripts, actions, and other unknown properties remain byte-for-byte
in the source. Moving a region exposes its tagged position only; this does not
claim to update undecoded absolute bounds or gameplay dependencies.

### NPD navigation

NPD v2 is interpreted as an 8-byte header, 26-byte waypoint records, and complete
6-byte adjacency entries. Export rejects malformed arrays or invalid endpoints.

Waypoint coordinates remain in native **navigation-map space**, not NIL/NSD
world-space. No guessed scale or Z-axis conversion is applied. Consequently,
waypoint markers are not guaranteed to align visually with level geometry.
Navigation/world-space mapping is a future reverse-engineering task.

Waypoint IDs are read-only and are not NSD entity IDs. Link endpoints refer to
zero-based waypoint **record indices**. Links are metadata points at the origin;
custom graph visualization is not implemented. Node/link counts cannot change.
Flags, color, extra fields, and edge cost are raw numeric fields; their complete
gameplay semantics are not asserted.

## GtkRadiant entity definitions

[radiant/entities.ent](radiant/entities.ent) supplies generic point definitions
for the custom classes. Merge its classes into the entity definitions of your
custom game pack configured for classic Quake III MAP syntax. This is not a
complete game pack, asset loader, or build-menu configuration. The descriptions
mark read-only metadata; compiler validation enforces the restrictions.

`nod_nsd_entity` regions with supported inline models carry `nod_bounds_min`
and `nod_bounds_max`: local-space AABBs computed from referenced LOD-0 mesh
vertices, not trusted header bounds or unreferenced vertex pools.
`nod_bounds_status` is `inline-mesh`, `no-inline-model`,
`unsupported-layout`, or `unsupported-parameter-type`. Unavailable bounds are
reported explicitly; they are never guessed from unrelated properties.
The region reader follows counted 36-byte modifiers, 28-byte motion frames,
and typed variable-length script parameters to locate the inline NOD.

The updated GtkRadiant entity module transforms these local bounds by the
editable native `nod_rotation` (MatrixPYR order) and world-space `origin`.
Rendering, ray selection, legacy brush culling, and region-save filtering use
the resulting world AABB. This is a bounding envelope, not exact trigger-shape
visualization or editable collision geometry. Re-export existing maps to obtain
region bounds; old sidecars remain compilable.

Other `nod_*` points use a 0.25-by-0.25-by-0.25 native-unit selection marker.
This fallback is not a physical bound or sector volume. Resolved native entities and legacy `misc_model`
aliases continue to use rendered source-mesh bounds and converted editor angles.
Waypoint markers retain raw navigation coordinates with no asserted world-space scale.
For enriched scenes, load `nod_types.def` alongside `entities.def`. Retain the
supplied `misc_model` definition for legacy scenes instead of a
duplicate generic game-pack definition. GtkRadiant recognizes this class for
dynamic per-entity model previews; the native metadata does not alter its editor
rendering.

The supplied [radiant/entities.def](radiant/entities.def) provides the same
bridge classes for game packs using the classic `.def` entity module rather
than the XML `.ent` module. Avoid duplicate `worldspawn`/`misc_model` definitions
when merging with an existing game pack.

### Native authoring asset placement

Keep the original hierarchy under a read-only game root:

```text
game_files/
  3D/Models/
  3D/Terrains/
  3D/Materials/
  Levels/
  Materials/
  Misc/Templates/Global.not
```

Set `nod_assetpath` in **NOD Authoring** (`scghost.game`) to that root.
Set `enginepath_win32` to the parent of the writable `nod` basegame directory,
e.g. `scghost-mapping`. Project `basepath` and generated NIL mesh root should
both be `scghost-mapping/nod`. This mounts the game tree read-only alongside
the writable mapping tree, without copying it into `scene-preview`.
`Materials/shaderlist.txt` supplies native NSA archives; the fork resolves
their images from `Materials` and `3D/Materials`. Preserve this hierarchy.
Merge the bridge definitions into the existing authoring `entities.def`,
avoiding duplicate `misc_model` or `worldspawn` classes.

The supplied local asset root is
`C:\Users\aknavj\Desktop\git\xboxrecomp-ghost\xboxrecomp\game_files`.
Level triplets come from its `Levels` directory; the compiler does not write
to it. `3D` is an asset namespace, not a destination for generated scene files.

### Fractional geometry and legacy preview-only game profiles

Version-4/5 model scenes have no brush planes and do not need the brush precision
workaround. For legacy scenes or standalone `NILtoMAP` output, enable
**Don't clamp plane points** (`NoClamp=true`) before opening exported maps.
GtkRadiant otherwise rounds the defining points to integers during brush
loading, even when interactive grid snapping is disabled. Keep **Clean tiny
brushes** disabled and avoid geometry edits/snap operations. Reopen the original
export after changing these settings; saving previously clamped geometry cannot
recover the lost planes.

Version-3 exports replace plane triples near the editor's binary32 normal
tolerance with well-separated coplanar points. This prevents a tiny but valid
face from being reported as a plane with no normal. The face planes are retained,
not the original three vertex coordinates used to define each plane. Very small
brush volumes can still be limited by the editor's winding/clipping tolerances.

For a preview-only profile, set its engine path to the **parent** of the basegame
directory. For example, a `basegame="nod"` project rooted at `scghost-mapping/nod`
needs engine path `scghost-mapping`, not the original extracted game root.
The project `basepath`, generated model/texture root, and VFS basegame root must
refer to the same directory. Provide a nonempty, writable `autosave` filename
in the project file.

Native `.nsa` archives are not Quake III shader files. A preview profile can
omit the custom `nod_assetpath` mount and use explicitly generated neutral
Quake III shaders for the exported material names. This avoids invoking an
incomplete native shader parser; it does **not** fix that parser or reproduce
authored game materials. Keep the native-authoring profile separate.

## Validation gates

Generated tests cover byte-identical no-edit compilation, exact NSD/NPD edit
ranges, sidecar integrity/source binding, supported MAP normalization, invalid
input, and rejection of unsupported changes.
Asset tests also cover template inheritance/empty overrides, exact resolution,
NOD/NMB stream layouts and member isolation, unsupported alias edits, version-4
binding integrity, cache protection, and noncommuting/gimbal-lock rotations.
NIL world tests cover raw source attributes, empty geometry, non-finite data,
immutable model/pose metadata, and compilation without generated model files.
Precision tests check the actual binary32 cross-product tolerance and oriented
half-spaces, plus backward compatibility with version-1 and version-2 sidecars.

Regression sources and their CMake entry point are local-only and excluded from
commits. They are not part of the production solution. With the local sources
present, configure a separate build from the SDK root, reusing existing GLM
package settings if needed:

```powershell
cmake -S .\src\tools\tests -B .\build\local-regressions
cmake --build .\build\local-regressions --config Release
ctest --test-dir .\build\local-regressions -C Release --output-on-failure
```

Opt-in validation against a local extracted game directory:

```text
python src/tools/tests/verify_scene_assets.py path/to/NODscene path/to/Levels
```

Pass `--level name.nil` (repeatable) to restrict the run. The script uses temporary
outputs, compares untouched files byte-for-byte and by SHA256, and verifies that
an NSD transform edit and an NPD waypoint edit change only their expected fields.
It never modifies the original assets.
Add `--assets-root extracted-game-root` to verify enriched scenes and
`--bundle level.nil=level.nmb` (repeatable) for explicit member lookup.
With enrichment enabled, the NSD edit targets a resolved native per-type entity
(or a `misc_model` alias in legacy scenes).
Native references and frame bounds are checked, with no generated prop meshes
or textures. `--native-checker executable` can additionally invoke the fork's
native loader for each asset/frame/member binding (three positional arguments).
An optional `--pico-checker executable` invokes a local OBJ/MTL validation
executable for each NIL-world OBJ preview.

These checks establish binary preservation and restricted patching, **not**
correct edited gameplay. Real GtkRadiant save/reload, engine loading, trigger
execution, prop behavior, and AI navigation must still be validated in the game
with staged copies. Geometry/BSP rebuilding and full native gameplay/property decoding
must be completed before arbitrary new levels can be compiled.

The implementation was validated against seven complete triplets in a locally
supplied Xbox game asset tree: all 21 untouched output files matched their
originals byte-for-byte and by SHA256. NSD position and NPD waypoint edits in
each triplet changed only their expected field bytes. Exposed NSD entity counts
matched each source's header count. `menu.nil` was excluded because it had no
complete matching triplet. No proprietary test assets are included in this
repository, and no GtkRadiant or game execution is claimed by these results.

Enriched validation also passed all seven triplets: 1,540 of 3,349 native records
had verified source-mesh aliases; 1,794 had no model, and 15 references in Hive
StationOuter B remained explicitly `missing-asset` under the selected catalog
and level-bundle scope. All 21 no-edit outputs remained byte-identical, and alias
position/navigation edits changed only expected fields. A temporary harness
built against the local GtkRadiant PicoModel OBJ loader performed 342 OBJ/MTL
loader checks across those levels, including all 40 distinct previews from
Miner Bunker A and their neutral material bindings.
This verifies loader compatibility, **not** an actual GtkRadiant GUI save cycle.

The subsequently supplied GtkRadiant log exposed automatic integer plane
clamping and 108 additional faces below the editor's normal tolerance in the
Miner Bunker A export. After the stable-plane fix, all 123,180 defining planes
passed a reproduction of GtkRadiant's binary32 normal check. The corrected
version-3 preview and the original version-1/2 exports all compiled to the same
native source bytes. A preview-only editor profile was prepared with unclamped
coordinates, a writable autosave path, matching asset/project roots, and neutral
shader aliases; another actual GUI load/save remains required.

The model-based version-4 Miner Bunker A preview has zero brushes, 118 native
model aliases, and one NIL world preview containing 24,636 source triangles.
Its MAP is 226,424 bytes. All 41 distinct OBJ/MTL previews passed the actual
GtkRadiant PicoModel loader, including the world's exact triangle count and
neutral material binding. Both new scenes and archived version-1/2/3 scenes
compiled byte-for-byte and SHA256-identically; supported native placement and
waypoint edits changed only their expected bytes. Original game inputs were
unchanged. Actual GUI save/reload and game behavior remain separate gates.
Version-4 enrichment and restricted-edit validation also passed all seven real
triplets: all 21 untouched outputs were identical, and all seven NIL world
meshes plus native aliases passed 349 PicoModel OBJ/MTL loader checks.

Diffuse-textured enrichment subsequently passed the same seven triplets and
349 loader checks, with all 21 native outputs still byte-identical. The
regenerated Miner Bunker A preview binds 106 native shaders to 71 distinct
diffuse images. Only `notex`, `invisible_scan`, and `HackConsole_bink` lack a
usable diffuse reference and remain explicitly white. Its NIL world has 92
diffuse surfaces and retains all 24,636 source triangles. An asymmetric UV probe
through the actual PicoModel loader verified the native V direction.
