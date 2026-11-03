# NILtoMAP, MAPtoNIL, MAPtoNSD, MAPtoNPD, NODscene and NODtoNMB

A standalone C99 CLI that converts NIL v35 render sectors into a Quake III
classic `.map` file for GtkRadiant. `NILtoMAP` itself does not link the C++ parser
library or OpenGL.
Use `NODscene import` for NOT-driven Radiant authoring. It combines NIL geometry,
NSD placements and the live NOT database. Legacy export/preview commands retain
their source-preserving NOD/NMB preview workflow.
`NILtoMAP` and `nil2map.sh` remain separate, optional triangle-brush utilities.

CLI help and editor hints show syntax, actions and essential limits. Detailed
workflows and format constraints stay in the documentation.

## Source layout

- `nil2map/`: export-only CLI and scene export/preview workflows.
- `map2nil/`: world-only NIL, template-placement NSD, manual-navigation NPD and source-preserving edit compilation.
- `nod2nmb/`: standalone native model-bundle compiler CLI.
- `common/`: shared scene types, MAP parsing/writing, source archives, geometry,
  asset/material helpers, I/O utilities, and the compatibility CLI dispatcher.
- `radiant/`: editor entity definitions.
- `tests/`: local-only regression sources and their separate CMake entry point;
  excluded from commits and not required by production builds.
- `editors/`: C++ NSD/NOT tree editors and their shared, lossless document model.

Configure `src/tools` as before. Existing command syntax and legacy MAP/sidecar
formats remain compatible; the new `import` command emits versioned authoring
entities rather than synthetic NSD classes.

### Shared implementation and source conventions

`common/tool_cli.h` and the `nod_tool_cli` library own Windows Unicode argument
decoding, ASCII filename-extension normalization, protected-output validation,
input/output alias checks, and exact, bounded binary reads. The three MAP
compilers use `nod_scene::AuthoringMap` for parsing and lifetime management,
including cleanup of partial parses and safe replacement on successful reload.
MAP inputs remain limited to 64 MiB; `NODtoNMB` retains its aggregate 256 MiB
input budget. Editors and the template catalog use the same argument and file
helpers rather than independent copies. Atomic publication continues to use the
existing editor workspace implementation.

These shared internals do not merge the commands: `NILtoMAP` is a direct geometry
importer, `NODscene` retains source-preserving workflows, the `MAPto*` commands
compile fresh authoring data, and `NODtoNMB` bundles models.

C/C++ tools sources follow the SDK `.clang-format` configuration: four-column
tabs, attached braces, pointer/reference alignment at the variable, and a
120-column formatting target. Headers use filename-based guards. New shared APIs
use the project naming conventions: snake-case free functions and variables,
PascalCase types, and camelCase public methods. Existing SDK parser interfaces
and namespace conventions are retained. Keep the corresponding header
first in implementation files and retain upstream notices alongside project
headers. GtkRadiant sources retain their existing module formatting.

All control-flow bodies use braces, including single statements. Separate
logical stages with blank lines while keeping initialization and its immediate
validation together.

The intended `map2nil` compiler must support both new maps and imported-level
edits: bundle referenced NOD models into NMB, link native placements, and build
NIL/NSD/NPD outputs. That full compiler is not implemented by this source split.
The current `NODscene compile` command still requires a `.nodsrc` archive and
only applies validated edits; it does not build new levels or invoke the
standalone `NODtoNMB` bundle compiler automatically.

## Converting the selected test MAP

From the workspace root, run [convert_test_map.sh](../../../convert_test_map.sh)
with Git Bash on Windows or Bash on Linux:

```bash
bash convert_test_map.sh
# Optional original asset-root override:
bash convert_test_map.sh "/path/to/game_files"
```

The script finds existing `MAPtoNIL`, `MAPtoNSD` and `MAPtoNPD` executables in the standalone
or root Release build. It compiles a single snapshot of
`scghost-mapping/nod/maps/test_map_aknavj.map` into
`scghost-mapping/nod/compiled/Levels/test_map_aknavj.nil`, `.nsd` and `.npd`.
The original asset root defaults to `NOD_NIL_TEST_ASSETS` when set, otherwise
the sibling `../../xboxrecomp-ghost/xboxrecomp/game_files` relative to the
workspace root. An explicit argument takes precedence.

All three compilers must succeed before existing outputs are replaced. Temporary
files are cleaned on exit; the source MAP and original assets are not modified.
Final replacement is per file, not an atomic multi-file transaction.
NMB generation and gameplay-spawn validation remain separate.

## Manual waypoint authoring and MAPtoNPD

Build `MAPtoNPD` through either build configuration. Reload the updated SCGhost
entity definitions and use the updated Radiant executable.

1. Place `nod_npd_waypoint` point entities manually on traversable ground.
   Positions use the same world coordinates as the MAP; no scaling or Z compression.
   The editor assigns the first available `nod_waypoint_id`.
2. Select two waypoints, source first, then **Connect (Ctrl+K)**. This creates a
   `nod_npd_link` with `nod_from`/`nod_to` referencing waypoint IDs, not record
   indices. A line and arrow follow the nodes in XY and camera views and honor
   navigation/entity/path visibility. Select the link's midpoint marker to edit
   its endpoints, or delete that marker to remove the directed edge.
3. Reverse travel needs another link in the opposite direction. Moving a node
   moves its displayed connections without changing IDs. The midpoint marker is
   only a selection handle, not a path control point.
4. Edit waypoint IDs and link endpoints in the inspector. Copied nodes must be
   given unique IDs and links adjusted explicitly; duplicate IDs, dangling
   references and duplicate directed edges fail compilation, never auto-redirect.
   Delete incident links when deleting a node. Normal node/link flags must be 0.

```text
MAPtoNPD --workspace WORKSPACE --asset-root ASSETS room.map WORKSPACE\compiled\Levels\room.npd
```

New points carry `nod_authoring=npd-world-v1` and `nod_space=world`, with no
imported `nod_id`. Existing source-bound NPD imports keep their separate legacy
workflow. The graph compiler validates navigation records; other entity classes
are left to the NIL/NSD compilers. `MAPtoNSD` validates and excludes navigation
entities instead of attempting to spawn them.

### Generating navigation entities programmatically

The following complete MAP compiles with `MAPtoNPD` and creates two waypoints
with one directed link from ID 9 to ID 40:

```text
{
"classname" "worldspawn"
}
{
"classname" "nod_npd_waypoint"
"nod_authoring" "npd-world-v1"
"nod_space" "world"
"origin" "0 0 0"
"nod_waypoint_id" "9"
"nod_flags" "0"
}
{
"classname" "nod_npd_waypoint"
"nod_authoring" "npd-world-v1"
"nod_space" "world"
"origin" "10 0 0"
"nod_waypoint_id" "40"
"nod_flags" "0"
}
{
"classname" "nod_npd_link"
"nod_authoring" "npd-world-v1"
"nod_space" "world"
"origin" "5 0 0"
"nod_from" "9"
"nod_to" "40"
"nod_link_flags" "0"
}
```

All shown waypoint/link keys are required, including `origin` on links. Link
origins are editor selection handles; the graph uses the endpoint positions,
not the link origin. Use point entities without brushes or imported `nod_id`
metadata. IDs must be canonical decimal integers (no sign or leading zeros,
except `0` itself); flags must be exactly `0`. `MAPtoNSD` applies the same
navigation validation before excluding these entities from scene placements.
Its template-placement and archive requirements still apply separately.

The compiler emits fresh version-2 native records with grouped outgoing ranges,
resolved record indices, weak connectivity groups, absent associations (FFFF),
sector 0 and normal flags. Limits are 65535 nodes/links, IDs 0..65534, 255 outgoing
links per node and 255 components. Invalid or unrepresentable edge lengths fail.
An empty graph is a valid 8-byte NPD, not automatically generated navigation.
Workspace protection, Unicode paths, a 64 MiB input limit and atomic per-file
publication match the other standalone compilers.

`MAPtoNPD` regressions independently decode the bytes and check graph limits and
publication failures. With `NOD_MOPP_TEST_RUNTIME` and Unicorn, `PathNative` runs
the original graph loader, real sector lookup, direction normalization and
`cPath::Find`: a two-edge route succeeds, its one-way reverse and a disconnected
destination fail. Only bounded file/allocator/progress facilities are substituted;
path search and route construction are not mocked. This does not prove AI
movement, collision clearance or gameplay controller integration. Automatic
placement/linking, walkability checks and special traversal remain deferred.

## MAPtoNSD placement compiler

Build the `MAPtoNSD` target through either build configuration.

```text
MAPtoNSD --workspace WORKSPACE --asset-root ASSETS test_map_aknavj.map WORKSPACE\compiled\Levels\test_map_aknavj.nsd
```

This compiles fresh NSD v11 point placements, independently of `MAPtoNIL`.
It does not require a `.nodsrc` archive and does not change the source-preserving
`NODscene compile` workflow. Compile NIL and NSD separately; this is not a
transactional multi-file build or automatic NMB/NPD generation.

Supported entities use Radiant's `not-template-v2` metadata, or the legacy
`map-template-v1` placement marker. Class/template spelling, archive provenance
and cached native type must match the live NOT database. All placements must
share one canonical ordered archive context: Global first, then at most eight
named scene archives. The first loaded definition wins. Each archive uses the
workspace copy when present, otherwise the original asset root.

Native point types 0, 2, 7 and 8 are supported, plus the verified type-3
`cRegionTrigger` and `cObjectiveUpdate` authoring profiles described below. Type 8 is the original
`tContainer` class, including the `133_SupplyDepot` template; it retains its
native template identity and uses the same empty placement-tail profile.
Nova is compiled as the real
Nova template, not a synthetic `info_player_start` mapping. Editor angles use the
existing native conversion; positions are unchanged and sector is 0, matching
the single-sector world compiler. Supported imported modifier streams preserve
ordered references, payloads and unused bytes; current MAP transforms/sector
replace their prefix fields. Typed `nod_prop_<id>` overrides are appended after
that stream and must already exist in the effective template. Reference,
transform, sector and type overrides are protected. Malformed streams, unresolved
references, invalid editor bounds, stale metadata, unknown keys, brush entities
and unsupported scene settings fail explicitly rather than being dropped.

An unscripted placement includes empty per-placement frame, parameter, inline
model, sector-link, camera and lighting sections, plus the complete
sound/world-array/environment/sky trailer. The trailer remains empty unless
scene references, fog, far clipping or reverb are authored as described below.
Registered NOT-native handlers supply their validated parameter/frame bodies;
trigger regions also supply an inline convex model. Navigation is compiled
separately by `MAPtoNPD`. Existing
template resource references remain native references; their deployment and
player/controller initialization are not verified by this compiler.
In particular, a NOT template's model name is not proof that the asset exists.
For model movers, check both the visual model and any inherited `PHYSMODEL`;
also use actual model bounds when sizing shafts and positioning stops. The generic
`floatingPlatform` names absent assets in the shipped data. Conformance case 10
uses the asset-backed `floatingPlatform_432` and explicitly overrides its inherited
missing collision reference with the available visual model as a convex proxy.
Type-8 tests additionally execute the original `gameThing_New(8)` dispatch and
complete container constructor chain with only heap allocation substituted.
This verifies the container class/type, not its deployed model, inventory,
destruction logic or complete in-game initialization.

Publication uses the same workspace/original-asset protection and atomic-write
helper as `MAPtoNIL`. The output directory must already exist. Failed compilation
preserves the destination, the input is limited to 64 MiB, and Unicode paths
are supported.

`MAPtoNSD` tests cover synthetic archives, first-loaded precedence, workspace
fallback, imported order/padding, all editable storage types, rejection cases,
publication protection and the original seven-template set. Tests derive the
selected MAP's current placements without restoring older user-authored content.
With `NOD_MOPP_TEST_RUNTIME` and Unicorn available, original native scene/file
readers consume the entire generated stream for empty, multiple and selected
placements. Object creation, world addition, GUID assignment, finalization,
postprocessing and render free-list cleanup are substituted test boundaries:
this proves framing and factory handoff, not actual actors or gameplay spawning.
GPU rendering remains deferred.

### NOT-native trigger volumes and larger action graphs

`cAIFightPath` has explicit boolean hostility controls in the shared SDK schema:
parameters 4/5/6/7 mean hostile to Terran/Zerg/Protoss/player, respectively.
GtkRadiant displays those labels from the catalog; both ordinary and imported
authoring validate canonical `0`/`1` values. Existing defaults remain `0,0,0,1`
for compatibility. Selecting a friendly-sounding actor template does not make
the spawner friendly: native spawning clears these four AI hostility flags and
reapplies the authored values. For a player-allied, Zerg-fighting unit use
`0,1,0,0`; for its opposing player-hostile Zerg use `1,0,0,1`.
Native regressions execute the original flag setters, friendship classifier
and target-validity checks for both sides of the stress wave. Actor allocation
is substituted; live AI combat remains a separate conformance check.

The shared SDK behavior catalog now exposes `cRegionTrigger` on resolved native
type-3 templates, including `trigger_region`. GtkRadiant consumes the same
18-parameter schema, connection descriptors and editable local
`nod_region_min`/`nod_region_max` vectors; their wireframe follows the authored
bounds. These are body fields, not fabricated NOT MIN/MAX property overrides.

New regions currently require zero rotation, finite local bounds and at least
0.001 units of extent on every axis. Defaults are player-only, enter-enabled,
exit-disabled, one-shot and initially enabled. Parameter 16 can initially disable
a finish volume; an earlier region's enter output toggles it on. This supports
ordered checkpoint objectives without claiming registration in the mission HUD.
Native mission objectives and cinematics still require their mission data.

`cObjectiveUpdate` is a separate, directly attached native region handler, not
a `cRegionTrigger` preset. Its 17-parameter initial authoring slice exposes
initial enabled state (0), objective key to complete (1), objective key to show
(2), and a linked trigger target (6). At least one objective key is required;
each key is at most 31 ASCII letters, digits or underscores. Keys must already
exist in the active mission's `OBJECTIVE` entries in its deployed chronicle.
Compilation validates syntax, not membership or localization. It does not
create mission objectives or modify chronicles.

Player entry fires once while enabled, updates the requested objectives, then
triggers the linked target with `p0=1`. Nonplayers and repeated entries are
ignored. Native trigger input with nonzero `p0` arms the region and checks whether
the current player is inside; `p0=0` fires only when already enabled.
Briefings, savepoints, logbook unlocks and extra callbacks remain uneditable at
safe defaults for new regions. Geometry uses the same validated local bounds
and inline native model as `cRegionTrigger`; changing handlers preserves bounds
in the editor. Original executable callback tests verify filtering, dispatch and
one-shot state, not live mission HUD rendering or final collision.

The fourth placement modifier is the native region-class alias, tag 15, rather
than an ordinary sector modifier. Each region includes a complete rigid inline
NOD box: mesh data, empty marker/auxiliary streams, six convex planes, an explicit
sector-0 link, and empty light/camera sections. Omitting these model-tail fields
desynchronizes the native scene reader and can corrupt subsequent player-start
counts. Regression coverage executes the original model, hull-containment,
sector, light and camera readers, as well as native ordered-checkpoint callbacks.
GPU/shader/allocator boundaries are substituted; live gameplay is not implied.

`cCutscenePlay` supports both native type-3 regions and nonmodeled/noncolliding
type-2 helpers. The catalog specializes each owner's body: regions receive local
bounds and the complete inline model/tail; helpers receive no region fields or
model. Its 20-parameter subset exposes the deployed NCS basename (0), one-shot
flag (1, default 1), and finish/stop linked target (14). Names are 1..31 ASCII
letters/digits/underscores/hyphens, optionally ending in `.ncs` (case-insensitive).
The same syntax check applies to edited nonvideo source-backed resource names;
unchanged imported payloads are retained exactly.

Player entry requests region playback; native trigger input requests playback
without filtering the cause or payload. Neither owner starts automatically.
One-shot counts attempts, even if a request cannot play. Repeating is opt-in and
does not add an already-playing guard. The linked output sends `p0=0` on both
normal finish and explicit stop, not merely on entry. Startup/preload/video,
actor cleanup, actor positional gates and numbered timeline events stay fixed
at safe defaults for new authoring. Existing `.nods` sequences can trigger a
named cutscene owner already in the MAP; there is no new cutscene language.

Name validation does not resolve deployed files or transplant dependencies.
NCS resources may bind cast actors, animations, cameras, audio and mission data,
or request a level change. Native reader/callback probes verify the camera-only
`0_Test_Cutscene.ncs` fixture and handler dispatch; final camera rendering,
playback, input restoration and collision remain live-game conformance checks.

`cTimerTrigger` adds automatic periodic outputs on nonmodeled/noncolliding
type-2 helpers. Its ten parameters are two float intervals followed by eight
object references; there are no frames or region bounds. Interval A runs first,
then B/A alternate indefinitely. Every expiry triggers all connected targets
with `p0=0`; phases are not separate on/off outputs. The handler has no trigger
input, so neither buttons nor `.nods` sequences can start/stop it.
Intervals default to 5/5 seconds and accept 0.001..599.999: original scheduling
truncates to milliseconds and rejects deadlines ten minutes or more ahead.
Edited source-backed intervals use the same bounds; unchanged payloads remain
byte-exact. Adding timer owners beyond the shared 128-slot native timer pool
is rejected, counting existing imported owners, cycling damage regions and
sound-loop and gameplay-control startup timers too. Collision initialization
uses two slots when initially hidden, otherwise one; shader initialization,
automatic translation/explosions and timed initially enabled emitters each
use one. Other script timers
also consume that pool; this static check does not guarantee runtime headroom.

`cRegionSound` supports native type-3 regions with two parameters: reverb preset
(`-1` unchanged, otherwise 0..25) and optional deployed ambient cue (at most
31 ASCII bytes, no paths). Both default to unchanged; there are no frames,
trigger inputs or linked outputs. Primary-player entry changes the specified
settings. Player exit checks after 0.5 seconds and restores world defaults only
if no sound region remains occupied. There is no previous-region settings stack.
An empty world ambient cue does not stop an old loop; provide a real baseline
when restoration is required. Fresh and edited imported settings share cue and
reverb validation; unchanged legacy payloads remain byte-exact. Original callback,
sector-scan and inline-model reader tests do not establish audible playback.

`cRegionSave` adds type-3 checkpoint regions with two integer parameters:
objective marker `-1` (no registration) or `0..15`, and initial enable `0/1`.
Defaults are `-1/1`, with no frames. A trigger enables without saving, disabling,
overlap checking or rearming. Savable controlled-player entry consumes one
attempt before calling native `SavePoint`, then displays `SYS_SAVE_INGAME`
and advances the chronicle's current objective marker. Even a rejected request
consumes the attempt and shows the hint; neither the hint nor compiler success
proves persistence. The native save path needs a valid world/local player/mission.
It is not a guaranteed disk/profile write. Marker registration occurs at startup,
including on disabled regions; duplicate IDs overwrite. Marker 0 requests the
initial marker. Prefer -1 unless deliberately authoring a shared marker route.
Edited imported marker/enable settings use the same validation; unchanged legacy
values and zero/two-frame payloads roundtrip exactly. New regions use current-player
respawn placement, not authored respawn frames. Live save/load remains unverified.

`cRegionStealth` exposes a type-3 native location-stealth body, kinds `[2,1]`,
with no frames/outputs: finite float32 value (default 0.5, not a percentage)
and initially disabled `0/1` (default 0). Controlled type-7 entry writes the
unclamped value; each trigger toggles the gate without refreshing occupants.
Exit resets zero even when disabled or another stealth region is occupied.
There is no restore stack, adjacency scan or per-handler save/restore override.
Fresh and edited imported parameters share finite-float/boolean validation;
unchanged legacy flags remain exact. Existing `.nods` sequences can trigger
the region, but start/stop data is ignored: two triggers toggle twice.
Original callbacks, full region dispatch and raw state writes are verified.
The tested native visibility query remains unchanged; no AI/HUD effect or
invisibility percentage is claimed. Use instrumented state checks, not a
promised visual/detection change, for conformance.

`cRegionKillZerg` adds a type-3 `[3,3,3,3]` body with no frames. Its four
references are actor exemptions, not action outputs; new slots default to -2.
Entry ignores matching GUIDs and non-actors, then requests original native
death for actor kinds 0/7 whose actor-type flags include bit 0x8. Combined
flags qualify, including Zerg-typed players. Original death guards remain
effective; verified fixtures set the dead flag and zero health. There is no
one-shot, toggle, trigger input, exit action or startup overlap scan.
Exemptions are region-local. Fresh and imported reference edits use existing
reference validation; unchanged shipped -1 sentinels and opaque geometry
remain exact. Native dispatch/death state are verified, but initial-overlap
tracking, live loading and death presentation remain unverified.

`cRegionDamage` adds a type-3 twelve-parameter body, kinds
`[2,1,2,2,1,1,1,1,3,3,3,3]`, with zero frames/outputs. New defaults request
5 native damage units immediately on primary-player entry, then every 0.25
seconds while enabled/occupied. Native damage calculation and health subtraction
are verified; difficulty/resistance/AI modifiers still apply.
Trigger IDs 0/2/3 toggle, ignoring float data. Rapid re-enable while occupied
can duplicate pending tick chains; disabling does not cancel queued timers.
Both phase durations must be nonzero to cycle. Each gains a native 0.1 seconds,
so fresh/edited phase values are bounded to 0..599.899. Cyclic owners count
alongside timer triggers and sound loops against the startup 128-slot budget; active ticks
and other scripts need headroom. New kill modifiers/damage type remain fixed
at 0; unchanged imported legacy values are exact and those modifiers read-only.
The optional non-primary mode kills actor kinds 0/7 regardless of enable state
and opens type-8 entrants with force 10. Primary container mode instead opens
four referenced containers with force 999 and disables the region; references
are not outputs. Fresh/imported edits share scalar/reference validation and
invalid edits preserve outputs. Native dispatch, health/timing, editor Undo/Redo,
inline readers and exact source-backed triples pass; live tracking, HUD and
gameplay remain unverified.

`cFogSet` supports type-3 regions and unmodeled/noncolliding type-2 helpers,
with zero frames/outputs and kinds `[1,1,1,1,1,2,2,2,1]`. Defaults are exit
mode 1, linear fog, RGB 45/60/90, start/end 0/20 meters, transition 1 second,
smoke 0. Exit modes 1/2 restore/keep fog; triggers toggle a captured snapshot,
ignoring IDs and float data. Any kind-7 player can activate a region.
New/edited RGB integers are 0..255, mode/smoke 0/1, transition 0..599.999;
linear start/end must satisfy finite `0 <= start < end` in binary32.
Unchanged imported legacy values remain exact. Native scene interpolation,
capture/restore, effect-manager smoke writes and timer limits are verified.
Mode changes or zero duration apply immediately; concurrent regions are not
a restoration stack. Exit does not restore smoke or cancel queued smoke
callbacks. The Save/Restore override stores only its active flag, not its
captured fog snapshot. GPU handoff, lookup and scheduling are fixture
boundaries; live loading/rendered fades remain unverified.

`cSoundLoop` adds unmodeled/noncolliding type-2 helpers with zero frames/outputs
and kinds `[7,2,2,2]`. Parameter 0 is a nonempty deployed cue (at most 31 ASCII
characters, no path); the native wrapper strips the final extension. The other
three floats are read-only shipped defaults 5/10/1, not invented range/volume
controls: the original callbacks do not read them. BeginScene schedules one
0.1-second startup timer. Playback requests use the owner and native flag 1.
Triggers ignore ID/data: handle zero attempts playback; nonzero requests stop.
Stopping retains the cached handle, so later triggers cannot restart a
successfully started loop. EndScene requests stop and clears it. Failed starts
can retry on a later trigger, not automatically. Startup owners count with
timer triggers, cycling damage and weighted gameplay-control startup timers
against the shared 128-slot guard.
Imported cue edits share validation; legacy float edits are rejected, while
unchanged blank cues/legacy floats remain exact. Native callbacks, name
normalization, normal mixer stop flags, editor Undo/Redo and exact triples
pass; mixer playback/lookup/scheduling are fixture boundaries, and live audio
and spatialization remain unverified.

The registry now exposes 31 verified workflows, including these fourteen:

| Area | Native handlers and verified scope |
|------|------------------------------------|
| Effects/particles | `cParticleToggle` on an existing emitter; helper `cEffectAdd`, `cObjectExplosion`, `cRain` |
| Movers | Modeled type-2 `cObjectMove` two-frame out-and-back; `cObjectRotate` one native-degree pivot |
| Objects | Modeled `cObjectSetCollision`, `cObjectSetColor` (shader frames, not RGB); helper `cObjectRemove`; colliding MOVETYPE-3 `cPhysicsActivate` |
| Messaging | Helper `cHelpText`; helper/region `cPopupConversation` with contiguous briefing keys and last-key removal output |
| Transitions | Helper/region `cMissionNext`; region-only `cLevelChange` |

These are SDK-owned schemas consumed by generic editor code. Resource/key strings
use `resource=native-resource` and explicit `allow-empty=0/1`; syntax checks do
not resolve deployment, chronicles, shader frame counts or destination levels.
Fresh and edited imported parameters share bounds, fixed fields and cross-field
checks; untouched imported payloads remain exact.

Ordinary relay/sequence connections send native type 0, not an arbitrary type
selected by float data. Particle input toggles; rain stops only; effect mode
selects add-on-trigger versus remove-on-trigger. Explosion input spawns once and
cancels automatic timers. Move input starts while idle; auto-start adds one
second to parameter 1 (maximum 598.999). Rotation input toggles continuation,
not immediate stopping. Collision uses data0=1 for on/visible, otherwise off/hidden.
Removal references are targets to remove, not outputs. Help shares a global
UI and can become consumed after 120 seconds. Popup completion is a last-key
notification, not proof of successful playback. Next-mission delay is truncated
to whole seconds; qualifying region entry bypasses it. Every level-change input
toggles enable, with data0=99 additionally requesting an immediate transition.

Original callback and mover-wrapper fixtures pass, but resource/presentation,
physics/tracking, UI and mission handoffs are boundaries; live behavior is not
claimed. Five separate conformance maps cover cases 21..25. Complex lightning,
constraints, specialized sliding/frame movers, pickups and unrelated regions
are not added by this batch.

Each relay still has at most 12 action slots. NOT-native graphs now allow up to
64 transitive actions per activation, including action buttons as roots, and
reject cycles. The bound reserves half of the native 128-timer pool for actor
timers and other controls; it is not a guarantee against arbitrary concurrent
activations. The trigger-stress fixture checks exact 12-Zergling/3-sidekick
dispatch and repeated concurrent controls against that pool.

### Scene music, ambient and sky references

`MAPtoNSD` accepts these optional worldspawn keys:

| Key | Native field | Default |
|-----|--------------|---------|
| `nod_music` | Music reference (optional extension) | Empty |
| `nod_ambient_loop` | Ambient-loop reference (optional extension) | Empty |
| `nod_sky_model` | Sky-model reference (`.nod`) | Empty |
| `nod_sky_animation` | Sky-animation reference (`.nad`) | Empty |
| `nod_sky_unmasked` | Scene sky-stencil mode: `0` or `1` | `0` |

For example:

```text
{
"classname" "worldspawn"
"nod_music" "ExampleMusic.wma"
"nod_ambient_loop" "env_example_loop.wav"
"nod_sky_model" "MS_Sky_Refinery.nod"
"nod_sky_animation" "MS_Sky_Refinery.nad"
"nod_sky_unmasked" "1"
}
```

The names are illustrative: use references available in the deployed level's
assets. Each nonempty reference must fit in 31 ASCII bytes including any
extension; use only letters, digits, `_`, `-` and `.`, without directory paths.
Audio names must not start or end with a dot; extensions are optional and not
restricted to one format. Sky extensions are checked case-insensitively.
Original spelling is preserved on disk.
Empty or omitted keys leave their fields zero. Duplicate keys fail explicitly.
The fields can be authored independently; an animation name does not infer a
model, and an authored reference alone does not establish runtime rendering.

The compiler writes NUL-terminated, zero-padded 32-byte fields at offsets 16,
48, 128 and 160 respectively in the 192-byte post-object trailer. Apart from
explicit fog/far-clip/reverb settings below, other trailer bytes and placement
data are unchanged. No resource collection, existence check,
sky placement, shader change or sky animation playback is performed by the
compiler. `MAPtoNIL` and `MAPtoNPD` leave these scene-only settings to `MAPtoNSD`.
Verify asset deployment, audio playback and rendering in the target runtime separately.

`nod_sky_unmasked` accepts only canonical `0` or `1`. Omitted/`0` preserves
the original surface-mask behavior. `1` sets scene flag `0x2` in the word at
file offset 72: the original renderer primes the sky stencil across the entire
viewport before drawing the sky model. Use this for open rooms with no native
sky-mask surfaces; without it, a valid model reference can remain invisible.
This is a scene-header flag, **not** trailer `effectsFlags`. Other scene bits
remain zero in newly compiled scenes. Models/animations still need deployed
native assets, and gameplay sight modes can suppress sky rendering.

The matching debug executable's original scene loader consumes both audio
fields and strips their last extension. Native-reader tests verify that handoff
for empty, extended, extensionless and maximum-length references, with and
without placements. Its level-start code passes the second field to
`cGameMixer::PlayAmbient`; this is not a live playback test. Writing the music
field does not implement or override the game's mixer/alarm score-selection
logic, and music playback from this field has not been verified.

The native level sound bank is selected from the first **five characters** of
the level filename. `1_0_0_Test_Room` loads `Sounds/1_0_0.xsb` and
`Sounds/1_0_0.xwb`. `nod_ambient_loop` is a friendly cue in that loaded bank,
not a bank name or a loose music filename. Native score selection similarly
requests `music_stealth`, `music_combat`, `music_puzzle` or `music_stinger`
from that bank. Merely referencing a cue from `2_2_2.xsb` does not load it
for Test Room. Resource aliases must preserve matching XSB/XWB bytes and their
internal wave-bank identities; author into workspace copies only.

The optional `test_sky_audio_native.py` executes the original scene reader,
sky-stencil routine, level-bank name selection and audio dispatch paths under
the hash-pinned debug executable. GPU/file boundaries are substituted;
name-selection/dispatch probes stop at the next loader/XACT boundary.
These checks establish header mapping, a full-viewport sky quad without mask
surfaces, Test Room's wave-bank filename, and ambient/stealth cue dispatch.
They do not prove audible output or final GPU rendering.

Other than the fog/far-clip/reverb settings below, numeric environment fields, including
volumes, remain unsupported and zero.
The executable-matched PDB identifies the remaining trailer fields as uint32
`soundEffects` (offset 80), `musicVolume` (84), `ambientVolume` (88) and
`effectsFlags` (124). Native-reader probes verify their scene-member mappings.
Their valid units, ranges, effects values and flag bits are not established;
volume fields must not be treated as floats or assigned assumed percentage
semantics. The known ambient-start handoff passes the track name rather than
the volume fields, but this does not prove they are unused elsewhere.
No worldspawn controls are exposed for these four fields.

The verified **profile/mixer music-gain API is a different storage domain**:
the music block in `cProfile::ApplySettings` (`0x00436260..0x0043629C`) reads a
float at profile offset `0x50`, clamps it to `0..1`, stores the clamped value,
and forwards it to `cGameMixer::SetMusicVolume` (`0x0051AD10`). That setter
clamps and caches a float at mixer offset `0x8C`; an unchanged gain skips the
backend update. `APIUpdateVolume` (`0x00535450`) converts music gain to the
integer level sent to XACT category 4: `int(2000 * log10(gain))` above the
binary32 `1e-5` threshold, otherwise `-10000`. For example, gain `0.5` sends
`-602`, and gain `1` sends `0`.

The optional `MusicGainNative` test executes that profile block, setter and
backend conversion unchanged, substituting only the final
`IXACTEngine_SetMasterVolume` call and stopping before unrelated profile
settings. It verifies clamping, cache behavior, attenuation values and the
threshold's next-float boundary. Use `NOD_MOPP_TEST_RUNTIME` and a test Python
interpreter with Unicorn, as for the other optional native checks; absent
prerequisites are reported as skipped. This does not verify audible output,
ambient-volume behavior, or a connection from either NSD volume word to the
mixer API. Do not copy these float units into the unsigned NSD fields.

NSD sky-model references are separate from DDS-backed archived
surface-material references passed through by the NIL compiler.

### Scene linear fog

`MAPtoNSD` accepts this optional group of worldspawn keys:

```text
"nod_fog_mode" "linear"
"nod_fog_color" "40 50 60"
"nod_fog_start" "20"
"nod_fog_end" "300"
```

`nod_fog_mode` is `none` or `linear`, case-sensitive. Omitted mode or explicit
`none` preserves the zero-filled default and rejects color/distance keys rather
than silently ignoring them. Linear mode requires all three remaining keys.
Color is three finite RGB floats in `0..255`, not normalized `0..1` values.
Distances are finite binary32 values in MAP/native world units, with
`0 <= start < end` after binary32 rounding. Missing, invalid, duplicate or
inconsistent settings fail without replacing the destination.

The compiler writes native mode `1` at trailer offset 96, three RGB float32
values at 100, and start/end float32 distances at 112/116. All other fields
remain unchanged. `MAPtoNIL` and `MAPtoNPD` ignore these scene-only settings.
Native tests execute the original scene loader and fog setter, verifying
linear render-state selection, color conversion and distances with the GPU
color call substituted. This is not a visual rendering test. Exponential fog
remains unsupported; far clipping and reverb are described below.

### Scene far clipping

The optional worldspawn key `nod_far_clip` sets the camera far-clip distance:

```text
"nod_far_clip" "600"
```

Use a finite nonnegative binary32 distance in MAP/native world units.
Omitting the key or setting it to `0` preserves the native default: the matching
debug runtime chooses 300 units for a zero field. Empty, negative, nonfinite,
overflowing, underflow-to-zero and duplicate values fail without replacing
the output.

The field is one float32 at trailer offset 120; all other fields are unchanged.
It is independent of fog and physics bounds, and does not configure the near
plane. Choose an explicit distance beyond the target camera's near plane.
`MAPtoNIL` and `MAPtoNPD` ignore this scene-only key.

Native tests execute the original scene loader, level-start far-clip branch
and local-camera setter with synthetic bindings for two cameras. They verify
authored distances and the zero/default fallback in both camera records, not
projection-matrix construction or visible rendering.

### Scene reverb

The optional worldspawn key `nod_reverb` selects a native reverb preset:

```text
"nod_reverb" "1"
```

Use a canonical decimal integer in `0..25`, without a sign, leading zeros,
whitespace or alternate numeric notation. Omission or explicit `0` preserves
the current zero-filled default. Preset IDs are native runtime identifiers;
no named EAX aliases or assumed "off" meaning for preset 0 are assigned here.
Invalid or duplicate settings fail without replacing the destination.

The compiler writes one uint32 at trailer offset 92 and leaves all other fields
unchanged. `MAPtoNIL` and `MAPtoNPD` ignore this scene-only key.
Native tests execute the original scene loader, level-start handoff, mixer
setter and backend preset selection with synthetic mixer/engine bindings.
Only the final XACT listener call is substituted in that handoff path.
They verify the selected native preset-table entry, not audible reverb;
runtime effects can also override reverb behavior.

### Generating template placements programmatically

For a template named `ExampleActor` defined in `Global.not` with effective
native type 0, the following complete MAP compiles with `MAPtoNSD`:

```text
{
"classname" "worldspawn"
}
{
"classname" "ExampleActor"
"nod_authoring" "not-template-v2"
"nod_template" "ExampleActor"
"nod_template_archive" "global.not"
"nod_template_archives" ""
"nod_template_type" "0"
"origin" "1 2 3"
"angles" "0 90 0"
}
```

`ExampleActor` is illustrative, not a shipped template. Replace its name,
defining archive and effective native type with values from the live NOT
database. The compiler does not create templates or infer missing metadata.
Template names are 1..31 bytes and must match the native spelling exactly.
All shown placement keys except `nod_template_archives` are required; omitting
that key is equivalent to an empty context (Global only).

For the accepted legacy form, change only these two keys:

```text
"classname" "nod_template_instance"
"nod_authoring" "map-template-v1"
```

`nod_template_archive` names the archive that actually supplies the selected
definition, using its canonical lowercase path relative to `Misc/Templates`.
`nod_template_archives` is the semicolon-separated ordered list of additional
scene archives, for example `a.not;b.not`. Use canonical lowercase names with
explicit extensions, no duplicates or trailing separator, and at most eight
entries. Do not list `global.not`: it is always loaded first. Every placement
must carry the same ordered context, including placements defined in Global.
Workspace copies of archives take precedence over original-asset copies.

`nod_template_type` is the effective inherited native type, not an arbitrary
editor category; write it as a canonical unsigned decimal integer. Supported
point types are 0, 2, 7 and 8; type 3 requires the trigger-volume profile above.
`origin` and `angles` both require three finite
numbers. Positions are world-space coordinates; angles are Radiant editor
degrees, converted to native rotation during compilation. The example writes
native rotation `0 0 90`, position `1 2 3`, and sector 0. No brushes, imported
modifier stream or property overrides are needed for a fresh placement.
Compiling a player template placement does not establish gameplay spawn
initialization; see [scriptless single-player startup](#scriptless-single-player-startup).

### Scriptless single-player startup

A successfully compiled player-template placement is not a guaranteed
single-player spawn point. `MAPtoNSD` writes the placement's native reference,
rotation, world position and sector; it does not generate startup scripts or
sequences, move the active player, or validate controller clearance. Do not
assume that placing `ghost`, Nova or an editor `info_player_start` initializes
the game's active player. `info_player_start` is not a supported native
template-placement shortcut.

**Reported compatibility case, not an automated SDK guarantee:** a user testing
SDK 0.6.7 output with a PC port of the May 2004 Xbox build observed a correctly
compiled `ghost` placement away from the origin, while the active player
remained at `(0, 0, 0)`. Their investigation associated this with
`gameMultiplayer_PlacePlayer`; the controlled native checks below establish
its more specific routing conditions. Original-level startup
could instead move the player through scripts or sequences, including
`ScriptThing::JumpToPlayerStart` and `tSequence::DoTeleToStart`.
This observation is specific to that build and startup path, not a promise
about all ports, modes or scripted levels.

In the same report, the origin was embedded in a wall/floor corner. The
player's position became NaN on the first frame, followed by
`tPlayer::OutsideHavokWorldEvent`, death and the death menu. Moving the map so
the origin was inside an empty room allowed the player to land on the generated
floor and move normally. Increasing physics bounds alone did not resolve the
failure. These observations implicate embedded startup in that case; they do
not identify the exact numerical failure or establish general player/dynamics
compatibility.

**Native collision-start reproduction:** `CollisionStartNative` compiles the same
solid brush spanning `(0, 0, 0)` to `(2, 2, 2)` with physics margins 1 and 100.
Only the serialized bounds differ; the original loader applies both sets of
bounds to the native physics world. An original native dynamic sphere of radius
0.25 starts with finite position/velocity, then the original contact generation
and constraint solver run with gravity `(0, 0, -9.8)` and a `1/60` timestep:

- Centers `(0, 0, 0)` and `(0.3, 0.3, 0)` produce NaN in all position/velocity
  components on the first step, with either margin.
- Center `(1, 1, 2.6)` stays finite for 180 steps and settles on the upper face
  at `z = 2.3`, including the mesh's 0.05 collision skin. The two margins give
  equivalent clear-start trajectories within `1e-5`.
- Partial overlap at `(0.1, 0.1, 0.1)` remains finite for the tested 60 steps
  with both margins. Overlap alone therefore does not imply this failure, nor
  does this control establish safe recovery from arbitrary penetration.

This is a deterministic reproduction of the current native test-body failure,
not a fix or a general physics-correctness pass. Tests explicitly assert the
known NaN cases. They reuse the hash-pinned Unicorn physics harness now kept
under the repository-root `tests` directory; file/allocation/GPU/progress/timing
facilities are substituted, but contact generation and solving execute
unchanged.

For [issue #2, item 4](https://github.com/aknavj/nodsdk-scghost/issues/2),
read-only hooks now identify a numerical failure in this test-body path.
`hkSphereTriangleAgent::processCollision` (`0x006355B0`) computes a reciprocal
distance at `0x0063575E`. When the sphere center and closest triangle point
coincide, the squared separation is zero and the stored negative reciprocal is
`-inf`. At `0x0063578B`, multiplying the zero separation vector by that value
produces a contact normal with three NaNs, observed immediately afterward at
`0x0063578E`, before the solver makes the body state non-finite. Both margins
reach this condition. The clear-start and partial-overlap controls instead
produce finite unit normals from positive separation distances.

This identifies zero-distance normalization in the native sphere path, not a
proven numerical cause for the full-player report. The tests do not exercise
the player/controller or the game's outside-world/death event path. No
collision data is jittered, physics code patched, or runtime asset modified;
the finding remains open for an actual player/controller fix.

Run the optional `CollisionStartNative` CTest with the same runtime and Unicorn
requirements as `PlayerSpawnNative`, or run
`python tests\test_collision_start_native.py .build\cmake\Release\MAPtoNIL.exe -v`
from the repository root using a Python interpreter with Unicorn installed.
Missing runtime/dependency prerequisites are reported as skipped, not as a
reproduction pass. Neither the executable nor original assets are modified.

**Native placement routing:** an optional, hash-pinned x86 oracle executes the
original `gameMultiplayer_PlacePlayer` at `0x0051F7D0` and
`tWorld::FindPlayerStart` at `0x00533C60`. The decision is not a direct
single-player/multiplayer check:

- Placement flag `0x8` or nonzero `mainSave_bRestoring` selects player-start
  lookup. A matched record is handed to `tThing::JumpToStart`. A failed primary
  lookup retries with first selector 0; if neither finds a record, the routine
  resets the player transform to identity at the origin and uses the world's
  first sector.
- Otherwise, it queries a sector at the player's current position. Finding
  one preserves the complete transform. Failing that query resets the transform
  to identity at the origin and uses the first sector, even when the previous
  position was nonzero.

The tests use synthetic player/world/start records and substitute
`FindPosSector`, `WorldAdd`, `SectorLink` and `JumpToStart` boundaries.
Player-start selection and transform-reset instructions execute unchanged.
This proves routing and fallback behavior, not actor construction, actual
teleportation, spatial-query correctness, controller clearance or physics.
These runtime flags are not MAP authoring keys and are not changed by the SDK.

Run the optional `PlayerSpawnNative` CTest with `NOD_MOPP_TEST_RUNTIME` pointing
to the matching original asset directory and Unicorn installed for the test
Python interpreter. Missing prerequisites are reported as skipped, not as a
native verification pass. Original executable code runs only inside Unicorn;
the original assets remain untouched.

For an initial scriptless smoke test:

1. Keep the entire native player/controller volume around `(0, 0, 0)` clear
   of collision, with a floor below it and walls away from it. A clear point
   alone is insufficient if the controller's shape still overlaps a brush.
   Keep the origin inside the physics bounds as well.
2. If the intended start is elsewhere, work on a copy of the MAP. Translate
   world brushes, template placements, lights, waypoints, link selection origins
   and other world-space authoring data by the same offset so the intended
   start becomes `(0, 0, 0)`. Update explicit `--physics-bounds` coordinates if
   used; automatic bounds follow the translated geometry.
3. Recompile NIL, NSD and NPD from the same translated MAP snapshot and deploy
   the matching outputs and required assets. Do not combine a translated NIL
   with stale placements or navigation.
4. Record the runtime build and startup mode, intended placement position,
   actual initial player position, first physics-step position and any
   `OutsideHavokWorldEvent`. Check landing and movement, not just level loading.

For a manual regression, retain separate embedded-origin and clear-origin
variants of the same small level. Hold runtime, assets and compiler options
constant, rebuilding each variant's matching outputs. Compare initial
positions, first-step finiteness and landing/movement. Test bounds widening
separately rather than changing it together with collision clearance. These
are reproduction steps, not an SDK test already executed or a guarantee that
the embedded variant fails on every runtime.

The compilers intentionally do not relocate levels or warn solely because a
template named `ghost` is away from the origin: scripted starts and runtime
initialization can make that placement valid. Native query and test-sphere
solver checks are not substitutes for this full-player smoke test.

## MAPtoNIL world compiler

Build the `MAPtoNIL` target through either the SDK or standalone tools build.

```text
MAPtoNIL --world-only --workspace WORKSPACE --asset-root ASSETS test_map_aknavj.map WORKSPACE\Levels\test_map_aknavj.nil
```

Physics bounds come from the compiled world box plus one MAP unit on each side,
rounded outward when float precision requires it. Excluded entities do not
expand the box. `--physics-margin N` changes that automatic padding on all six
sides; use a positive finite number in MAP world units. The default remains
`1`, preserving existing output. For example, `--physics-margin 100` reserves
100 units beyond the geometry for objects that may leave the compiled world box.
Padding remains subject to float32 precision; bounds are rounded outward when
needed to strictly enclose the geometry. Unrepresentable bounds fail compilation.
`--physics-bounds MINX MINY MINZ MAXX MAXY MAXZ` instead sets all six coordinates
explicitly and must strictly enclose the world. It cannot be combined with
`--physics-margin`; neither option changes the source MAP.
A larger margin is not a fix for spawning embedded in collision, and no
particular margin establishes full player/controller or arbitrary dynamics
compatibility; see [scriptless single-player startup](#scriptless-single-player-startup).
The workspace, asset root and output directory must exist. Output stays inside
the workspace, outside original assets, and cannot alias the input MAP.
The compiler validates the complete image before atomic replacement; failures
preserve an existing output. MAP input is limited to 64 MiB.

`--world-only` acknowledges exclusion of all non-world entities. The summary
reports their count. No NSD, NPD or NMB is emitted; player entries and placed models
are not compiled. Authored static lights are handled separately as described below.
GPU rendering and full-game smoke tests
are deferred, not publication requirements.

### CPU direct-light baking

See the [native lighting technical document](../../docs/NATIVE_LIGHTING.md)
for the format, equations, implementation details and validation procedure.

Place **nod > nod_static_light** point entities in empty space using Radiant's
entity browser. Reload the updated definitions and executable first.
Convert normally with **Bsp > Convert NOD level...** or the command above.
Lights are MAP-only authoring helpers, not NSD template placements.

| Property | Meaning | Default |
|----------|---------|---------|
| `origin` | World position | Placement position |
| `_color` | RGB channels, each 0..1; not all zero | `1 1 1` |
| `light` | Intensity, greater than 0.2 and at most 1024 | `8` |
| `radius` | Attenuation reference distance, 0.01..65536 world units | `10` |

`radius` sets the falloff curve, not a hard cutoff. The native type-0 graphics
setup uses range 9999. Worldspawn `nod_light_spacing` optionally sets the maximum
triangle-edge length, 0.01..1024 world units, default `4`. Smaller values give
finer shadow sampling but more triangles. Shared-edge subdivision preserves
closed brush geometry and regenerates the matching collision mesh/MOPP.

The compiler bakes Lambert direct light, native-compatible point attenuation
and brush shadow rays into vertex colors, and emits matching native sector
lights for moving characters. Limits are 64 lights, 4096 world triangles,
65535 mesh vertices and 64 million shadow-query work units (spatial-tree visits
and candidate triangle checks). The CPU baker builds a bounding-box hierarchy
once per bake to skip unrelated occluders, preserving the original segment
bounds and triangle-intersection tolerances. Exceeding a limit
fails explicitly without replacing the previous NIL; increase sample spacing
or reduce lights. Unsupported/duplicate light properties are rejected.

Extended conversion options support four samples per vertex, optional brush
shadows, Phong-angle normal smoothing, one directional sun/moon, and explicit
ambient lighting for surfaces and native characters. For example, append:

```text
--light-samples 4 --light-phong 89 --sun-light 1 1 1 1 -1 2 -4 --ambient-light 1 1 1 0.2
```

GtkRadiant's Full preset enables four-sample oversampling. Its shadow work is
greater than a one-sample bake, but spatial filtering avoids charging every
world triangle to every ray. The compiler does not silently lower sample count,
disable shadows or increase the work budget to make a bake succeed.

CLI RGB uses 0..1 and intensity multipliers; the dialog displays RGB 0..255
and intensity percent. Ambient strength is measured before native attenuation
(approximately 0.1632653), consistently for surfaces and characters. The initial
extended exporter incorrectly canceled this factor; regenerate NIL outputs
made with that version to correct overly strong ambient.
`--lighting auto|on|off` defaults to auto.
`--light-spacing N` overrides the MAP setting and `--light-shadows on|off`
controls brush rays. `--lighting off` preserves the old white output even
when the MAP contains lights. No override modifies the MAP.

There is no indirect bounce, light-style authoring or model/entity shadow
occlusion. Character lighting follows native selection;
the current single-sector world does not prevent character lights from
crossing walls. Brush shadows are vertex-sampled, not texture lightmaps.
Maps with no authored lights retain the original white, zero-light baseline.
The compiler never inserts lights or modifies the source MAP.

### Supported world data

The initial fixture is `scghost-mapping/nod/maps/test_map_aknavj.map`.
Output includes generated native collision/spatial data, not only render
sectors. The SDK's `NILCollision`
decoder reads the no-terrain Havok mesh/MOPP framing and surface mappings.
`mopp_compiler` builds deterministic single-subpart MOPP programs, limited to
4096 triangles per body. `world_geometry` unions classic convex brushes into
exterior polygons and builds their collision mesh/MOPP. It retains source
material/texture fields; earlier brushes own coplanar overlaps. Seam vertices
are shared and triangulated without T-junction cracks.
The selected MAP produces 32 exterior surfaces, 32 vertices and 72 triangles,
removing 88 square units of internal faces. Non-world geometry and template
placements remain excluded from the world mesh; static lights become sector
records. `world_render` adds classic UVs, hard-normal/UV seams and
canonical native shader bindings: 136 render vertices, 72 triangles, 2 materials.
It uses Radiant's source-plane float math and actual editor image dimensions,
including `qer_editorimage`. Editor archives follow `Materials/shaderlist.txt`;
native level-archive overrides do not change editor dimensions. Missing unrelated
shaderlist archives warn, as in Radiant. Requested images must resolve to valid,
unambiguous 2D DDS files; no white preview fallback is used. Standalone NAS
shaders and non-DDS editor images are unsupported.
`world_sector` encodes one primary-pool sector, merges adjacent material runs
and retains collision triangle starts. It accepts DDS-backed direct shaders
and native NSA material references, with zero authoring flags only. For an
archived material, the NIL shader table receives the canonical native material
name, not its diffuse DDS basename. An archived definition takes precedence
over a direct DDS with the same name; that collision no longer causes rejection.
The historical `scripted` material-info field records archive provenance, not
the presence of scripts.

Archived materials still need a resolvable, supported diffuse DDS and editor
image. Existing ambiguity, DDS-layout, 1..31-byte native-name and geometry
checks remain in force. `qer_editorimage` continues to determine texture
projection dimensions, independently of native level-archive overrides.
DDS-backed `shader default` and `shader sky` entries can pass through by name,
but the SDK does not compile their shader programs, collect archives/textures,
or validate every native shader's rendering behavior. Deploy the required native
banks and resources separately; an NSD sky-model reference is a separate setting.
Materials without supported DDS images and standalone NAS shaders remain
unsupported.
Native reader/fallback-draw tests check record
bytes and draw inputs with mocked file, allocator and GPU boundaries.
`world_image` assembles a complete NIL image in memory, including generated
zero-light/ambient header records, sector-owned mesh/MOPP and group mappings.
Mapping tails contain one sector presence word and one presence word per
render surface group. A group's collision offset follows only when that
group's presence word is nonzero; an absent group has no offset placeholder.
The sector presence word does not suppress the per-group records. The current
compiler maps every generated group, so this conditional serialization leaves
existing all-present output unchanged.
`CollisionMapping` checks exact bytes and independent decoder consumption for
present, absent and mixed groups in one- and two-sector serialized fixtures.
Absent offsets must not consume the next sector's presence/group words. The
test rejects every truncated mapping-tail length and the old unconditional
offset layout. The two-sector fixture tests framing only; it does not extend
the compiler's single-sector authoring profile.
Its image API requires enclosing physics bounds and rejects mismatched MOPP;
the CLI computes those bounds unless overridden.
The selected room is 8220 bytes; excluded entity indices remain listed.
`WorldNative` checks original stream readers, postprocessing, sector lookup and
mesh rays. `PhysicsNative` constructs the native physics world, registers/binds
the body, and checks ownership, broadphase queries and gameplay rays.
The selected room passes all six interior boundary hits and an interior miss.
`SolverNative` checks original integration and contact solving with test spheres:
free motion, stable floor/ceiling/wall contacts and the 4096-triangle mesh.
`CollisionStartNative` compiles its own brush fixture and contrasts clear,
partially overlapping and exact-surface starts at two bounds margins. Its NaN
cases deliberately reproduce a known native failure, rather than certify a fix.
`ShaderNative` loads both room textures through original shader/DDS code, checks
exact base uploads, native mip generation and caching, and tests single-sector
visibility lists and group box-frustum selection. GPU resources are CPU-backed.
Automated validation of player/controller behavior, shader/visibility rendering
and complete game loads remains deferred and is not required to run the
world-only compiler. The manual compatibility report above does not extend
these automated test guarantees.
Geometry is limited to 4096 triangles before/after CSG. Fragment, polygon and
work limits reject excessive complexity or precision loss without changing output.

Tests: `WorldGeometry`, `WorldRender`, `WorldSector`, `WorldImage`, `MOPPCompiler`,
`NILCollision`, `MAPtoNIL`, and optional `MOPPNative`/`SectorNative`/`WorldNative`/`PhysicsNative`/
`SolverNative`/`ShaderNative`.
Set `NOD_WORLD_TEST_MAP` to the selected test MAP for its exact geometry checks
and native room-boundary queries.
Set `NOD_NIL_TEST_ASSETS` to check all eight original NIL files and the selected
MAP's real material dimensions (with `NOD_WORLD_TEST_MAP`).
`MAPtoNIL` compares the published test room byte-for-byte with the validated
image fixture and, when the native runtime is available, loads that file and
checks original gameplay rays against all six interior boundaries and a
180-step floor contact using automatic bounds.
For native checks, set `NOD_MOPP_TEST_RUNTIME` to the original asset root
containing the verified debug `Ghost.exe`; Python `unicorn` must be available.
The MOPP oracle emulates original AABB/sphere queries and compares accelerated rays
with the original unaccelerated mesh query, including CSG overlaps, seams and
rotated/sloped geometry. Room queries also check that former internal wall
faces no longer collide. It does not run the PE loader,
modify original assets, or establish full level compatibility.
`NILCollision` also exposes native-equivalent triangle-to-surface and sound
lookups; mapped offsets and material ranges are checked against original meshes.
The shared MAP parser has a separate authoring entry point that preserves
source order without requiring archived `nod_id` values. Existing roundtrip
parsing retains its identity checks and canonical ordering.

## NODtoNMB bundle compiler

`NODtoNMB` creates native **NMB version 2** bundles from existing **NOD version 10**
models. It also accepts NOC v10 collision models with the same verified binary
model layout: real level bundles contain both `.nod` and `.noc` members.
This packages model data; it does not compile world geometry, animations/NNB,
collision sectors, or native placements, and does not modify the MAP.

```text
NODtoNMB --workspace WORKSPACE --asset-root ORIGINAL_ASSETS WORKSPACE/level.nmb model.nod collision.noc
NODtoNMB --workspace WORKSPACE --asset-root ORIGINAL_ASSETS WORKSPACE/level.nmb --model NativeName.nod path/to/model.nod
```

Both roots must already exist. The destination must be inside the workspace,
outside original assets; its parent directory must exist. All input parsing,
validation, compilation and a re-read through the existing `NMBParser` finish
before publishing output. Publication reuses the native editor's atomic save:
invalid input or failed publication leaves an existing destination unchanged.
Symlink escapes and input/output hard-link aliases are rejected. Replacing a
workspace hard link to an unrelated original bundle replaces the link itself,
not the original data.

The default member name is the source filename, including `.nod` or `.noc`.
Names are case-insensitively unique, flat ASCII filenames (letters, digits,
`_`, `-`, `.`), at most 32 bytes. Names are never silently truncated or renamed.
Use `--model NAME FILE` only when an explicit alias is intended; native references
must agree with the chosen name. Input order is member/frame order and output is
deterministic. An explicit invocation with an output but no models creates a
valid empty bundle. Limits are 256 members and 256 MiB of total inputs/output.
`--` ends option parsing; `--help` prints usage.

The writer preserves vertex sets/types 0-5 (including separate TBN streams),
indices, all four LOD descriptors, per-set vertex bases, shader names, bone
palettes, native compact skeleton fields, group linkage, markers and optional
packed vertex colours. NOD marker 4x3 transforms become native 4x4 matrices.
NMB bones carry position, inverse translation, parent and joint ID, not the NOD
editor orientation/padding fields; this matches original skeletal bundle
records, including source models with nonidentity orientation matrices.
Header allocation uses fixed target-runtime structure sizes, not host `sizeof`
or serialized file length.

Unsupported headers, vertex types, blend-shape/reserved group extensions,
post-marker extensions, unrepresentable vertex advances, malformed indices,
invalid hierarchies and non-finite attributes fail explicitly. This is not a
generic NMB editing/re-serialization API.

Reusable SDK entry point: `nod::bundle::compile` in
[`common/nmb_compiler.h`](common/nmb_compiler.h), linked through the CMake
`nmb_compiler` target. It takes named byte buffers and returns NMB bytes or an
explicit error without performing filesystem I/O.

`test_nodtonmb.py` independently decodes the output and checks geometry, binary
attributes, member/frame order, limits, malformed inputs, Unicode paths and
atomic workspace protection. After configuring the separate local regression
build described below, opt-in original-asset checks use:

```powershell
$env:NOD_NMB_TEST_ASSETS = 'C:\path\to\game_files'
ctest --test-dir build\local-regressions -C Release -R '^NODtoNMB$' --output-on-failure
```

The local original-asset sweep verified 1,401 NOD and 419 NOC models. A rebuilt
43-member Miners Bunker bundle retained the original names/order and native
geometry, skeletons, markers and colours; all 43 members resolved and rendered
through Radiant's native geometry harness. The rebuilt bundle is not claimed
byte-identical: the standalone Overlord inputs differ from the shipped bundle
by one shader-name capitalization byte and seven unused-LOD vertex-count bytes.
The compiler takes those bytes from its inputs; active LOD records agree.
In-game execution remains unverified. Select generated bundles through Radiant's
existing per-map bundle selector; automatic MAP/NOT dependency collection and
integration into level compilation are subsequent work.

## Build

From the repository root:

The Bash wrappers build all tools, the shared C++ parser library, the viewer,
and the NSD/NOT editors.
They work from any working directory; use Bash on Linux/macOS or Git Bash on
Windows. CMake and C99/C++17 compilers are required. Linux/macOS also require
GLM, OpenGL, GLFW, and GLEW. On Windows, install Git and Visual Studio C++ build
tools with a Windows SDK; `build.sh` automatically bootstraps `.deps/vcpkg` and
restores the manifest dependencies, including `dirent`. The checkout survives
cleaning; initial setup requires network access.
`NODscene` uses the existing `asset_parsers` NSD/NPD and native NOD/NMB parsers.
Region entities with supported inline NOD meshes export computed local LOD-0
bounds. The updated GtkRadiant entity module uses them for dynamic display and
selection; other native points use small 0.25-unit markers. Model aliases retain
their rendered mesh bounds. See the scene bridge contract for schema-6/7 and
legacy-map compatibility.
NIL geometry, legacy OBJ extraction, and material helpers remain C during this
incremental migration.

```sh
bash build.sh
bash nil2map.sh --scale 32 --thickness 2 input.nil output.map
bash clean.sh
```

`build.sh` configures the root project in `.build` with the viewer and
editors enabled, then builds all applications as Release and copies only their
executables directly into `build-out/`. CMake files, libraries, and intermediates
remain in `.build/`. Windows uses `x64-windows-static` and the static MSVC runtime,
so no additional DLLs accompany the executables. Custom Windows toolchains must
also provide static dependencies.
`nil2map.sh` finds the executable in `build-out/` (falling
back to older standalone builds in `build/niltomap`) and forwards
all converter arguments and its exit status, without building automatically.
For batch conversion, use `bash nil2map.sh --asset-root ASSETS` with optional
`--scale N`, `--thickness N`, and `--classic`. The input can be the asset root
containing a `Levels` directory, or `Levels` itself (case-insensitive).
All regular `.nil` files are found recursively, including uppercase extensions;
relative paths below `Levels` are preserved under repository `maps-out/`.
Existing MAPs are replaced only after successful conversion. Other failures do
not stop the remaining conversions; a summary is printed and any failure gives
a nonzero exit status. Original assets are never overwritten. Empty or ambiguous
level directories, output-name collisions, and symlinked output paths are rejected.
`clean.sh` removes `.build` and `build-out` completely: all configurations,
CMake caches, generated projects, intermediates, binaries, and installed dependencies.
It also recognizes the older CMake layout in `build-out`.
It also handles empty or partially configured builds without requiring CMake,
but refuses symlinked or unrecognized directories and caches belonging to another
source tree. Other builds, exported maps, sidecars, and preview assets are untouched.
Keep custom toolchains outside `.build` and `build-out` and reapply non-default configuration
options after cleaning. The automatic `.deps/vcpkg` setup is reused without
manual toolchain configuration.
Each wrapper accepts `--help`.

The C/C++ sources use tab indentation, K&R braces, descriptive snake-case
internal names, and named native-format layout constants. The existing C99
helper/C++17 scene-tool split and public C interfaces are retained. Header
guards follow the project filename-based convention.
Production tool targets enable `/W4` or `-Wall -Wextra -Wpedantic`; when
the `CI` environment variable is set, warnings are errors.

For a tools-only build without viewer dependencies, use CMake directly:

```sh
cmake -S src/tools -B build/niltomap
cmake --build build/niltomap --config Release
```

If GLM is outside the normal search path, set `CMAKE_PREFIX_PATH` for
`build.sh`, or pass `-Dglm_DIR=<prefix>/share/glm` to CMake. Existing vcpkg
installations can be reused; see the [all-apps build instructions](../../README.md#build)
for `VCPKG_ROOT` and `CMAKE_TOOLCHAIN_FILE` overrides. Automatic Windows setup
bootstraps vcpkg when no explicit or cached toolchain is configured.

The tools-only executable is `build/niltomap/NILtoMAP` with single-configuration generators,
or `build/niltomap/Release/NILtoMAP.exe` with Visual Studio. Python 3 is optional
and only needed for local generated-fixture tests.

### Local-only verification

Regression sources and their test-only CMake entry point are kept locally and
excluded from commits. Production solutions do not depend on them. If these
local sources are present, configure and run their separate build from the SDK
root; use the same GLM package settings as the production build when needed:

```powershell
cmake -S .\src\tools\tests -B .\build\local-regressions
cmake --build .\build\local-regressions --config Release
ctest --test-dir .\build\local-regressions -C Release --output-on-failure
```

### Root project build

Alternatively, build through the root project without viewer dependencies:

```sh
cmake -S . -B build/cli -DNODSDK_BUILD_VIEWER=OFF
cmake --build build/cli --config Release --target NILtoMAP
```

Root builds place executables under `build/cli/cmake/` (with an additional
configuration directory for Visual Studio).
`NODSDK_BUILD_PARSERS=OFF` no longer disables the parser library when tools
are enabled: `NODscene` requires it.

## Usage

### Command-line help

Every tool's `--help` uses the same format, including the editors and viewer
(their help exits without opening a window):

```text
OpenNOD <toolname> | http://github.com/aknavj/nodsdk-scghost
Built date: <compiler build date> | version: 0.6.7

Help args:

--argument VALUE : Brief description.

Example usage:
<toolname> --argument value
```

Running a tool without arguments also prints this help when it requires inputs.
Missing-argument help goes to stderr with a nonzero exit status; explicit
`--help` goes to stdout and succeeds. NSDEditor and NOTEditor can intentionally
launch their GUI without arguments, so their existing startup behavior is preserved.

The C/C++ shared [help formatter](common/tool_help.h) owns the layout, and
[res/version.h](../../res/version.h) defines the version shared with Windows file metadata.
All application windows use the same shared title template:
`OpenNOD <tool> | http://github.com/aknavj/nodsdk-scghost | version: 0.6.7`.
The viewer identifies itself as NODAssetViewer; the editors use NSDEditor and NOTEditor.
Each tool documents its own supported flags, positional inputs, and example
commands. Build dates come from the compiler's `__DATE__` value, not the run date.
To validate every packaged application's help:

```sh
python tests/test_cli_help.py build-out
```

### Import an editable NOT-driven scene

```powershell
.\build\viewer\cmake\Release\NODscene.exe import input.nil input.nsd input.npd 'C:\Mapping\nod' 'C:\game' 'C:\Mapping\nod\maps\level.map' 'C:\Mapping\nod\maps\level.nodsrc'
```

Both outputs must be inside the existing authoring workspace and outside the
original asset root. They must be distinct files, including hard-link and
case-insensitive aliases. Output directories must already exist. Global loads first,
followed by the NSD archive order, with workspace overrides and explicit errors
for missing/cyclic definitions or unverified placement layouts.

Entities use actual NOT template classnames, `nod_authoring=not-template-v2`,
template/archive/type bindings, and ordinary editor `origin`/`angles`.
Classnames use the winning NOT definition's spelling even when an NSD reference
uses different capitalization; the original reference remains in the retained stream.
`nod_modifier_count` and numbered `nod_modifier_N` keys preserve complete ordered
36-byte modifiers as 72-character lowercase hex values; no long MAP tokens are
required. The native fourth-slot region-class alias is retained separately as
`nod_region_class`, never reinterpreted as global script property 15. Original
native bytes, including opaque tails and region data, remain in `.nodsrc`.
Inherited defaults are not materialized into local `nod_prop_ID` keys.
Radiant resolves the retained stream against its live NOT database; later edits
create explicit local overrides after that stream. No external NSD binding is
required. **The legacy `compile` command does not compile authoring maps or new
placements.** Geometry-only `NILtoMAP` remains available without scene data.
Undecoded NIL post-sector data is not converted into editable geometry and emits
a warning; its original bytes are preserved in the source archive.

### Template catalog and explicit shared-property saves for Radiant

`NODTemplates` is a headless C++17 tool built with the native document library,
even when `NODSDK_BUILD_EDITORS=OFF`. It does not require the GUI backend:

```powershell
cmake --build .\build\viewer --config Release --target NODTemplates
.\build\viewer\cmake\Release\NODTemplates.exe --workspace 'C:\SCGhostMapping\nod' --asset-root 'C:\game' --archive Shared --archive Mission.not
```

Global is implicit and first; up to nine additional archives load in argument
order. Names are normalized through `NOTDatabase`, must fit 31 ASCII bytes
including any implicit `.not`, and cannot contain quotes or semicolons.
Duplicates, explicit Global and unsafe relative paths are errors. Workspace
archives override originals; malformed/unreadable/ambiguous overrides do not
silently fall back. The same nonoverlapping workspace/asset policy as the
native editors applies. Catalog queries write no assets or workspace files.

Successful stdout is a complete XML document rooted at
`<nod-template-catalog version="1">`. Ordered `archive` elements contain canonical
`name`; `warning` elements contain `text`; each winning `template` has `name`,
`archive`, `type`, `properties`, `placeable`, `model` and `problem` attributes.
`properties` counts effective winning properties, not serialized modifiers.
`type` is uint32 property 20 metadata, not a verified gameplay enum.
`placeable="1"` requires complete inheritance resolution, that property and a
nonblank MAP-safe printable ASCII name of 1-31 bytes without quotes.
Unplaceable rows retain diagnostics; unsupported names use a display placeholder.
Non-ASCII/control bytes in diagnostic metadata are displayed as hex escapes;
XML attribute newlines/tabs and markup are escaped.
Errors produce stderr and a nonzero exit, with no partial catalog on stdout.

Add `--template NAME` to query one first-loaded template (case-insensitive).
Its `template` element adds `details="1"` and contains effective `property`
elements with `id`, `name`, `storage`, `value`, `archive`, `template`, `editable`
and `problem` attributes. Provenance identifies each winning property's owner,
including cross-archive and late-inheritance overrides. Storage is `string32`,
`float32`, `uint32 bits`, `vector3` or diagnostic `raw`. Values use the native
document serializer's formatting and validation. Structural IDs 0-3 and 20,
unsupported payloads, nonfinite values and MAP-unsafe quoted strings cannot be
edited; read-only entries include a reason. Missing/unsafe template names and
duplicate query options fail explicitly. Queries remain entirely read-only.

Add `--details 1` for detailed metadata for all templates, including a `parent`
attribute identifying only the first displayed base. Full ordered inheritance
still determines effective values. Radiant uses this to register real template
classes at startup and reload. Add `--template NAME --modifiers HEX` to resolve
a retained placement stream (1-128 complete 36-byte records) without opening an
NSD. Winning placement-local properties have `source="placement"` and an empty
archive; NOT winners retain archive/template provenance.

Radiant's standard **Entities** inspector consumes this database. Properties and
Effective Properties edit typed local overrides with Undo/Redo and removal,
without changing shared defaults. Old `nod_template_instance` markers remain
compatible but are not the normal creation model. The source-bound NSD query
below remains a legacy diagnostic facility. Explicit shared saves use a
separate Radiant dialog, not the instance Apply/Delete controls.
**This is not native export or build integration.**

An explicit existing-property shared save uses all five options:

```powershell
NODTemplates --workspace DIR --asset-root ORIGINALS --archive Extra --template OWNER --target-archive Global.not --write-property 139 --expect "1.5" --value "2"
```

`OWNER` must actually own the effective property in the exact ordered context,
and its source archive must match `--target-archive`. `--expect` is the previous
canonical native value returned by the query. Replaced owners, stale values,
protected/unsupported payloads and invalid typed values are rejected before
saving. Placement modifiers, bulk details and duplicate options are not allowed
in a write request. The native Document fixed-field editor preserves unrelated
bytes and unknown data. MIN/MAX changes additionally validate affected
descendants, so a parent's bounds cannot invalidate a child's opposite override.
The result is a detailed catalog for the owner with the canonical saved value.

Saves use the native workspace-only atomic writer. Original-source edits
create a corresponding workspace copy; active workspace copies are edited
in place. Concurrent source changes and newly appeared destinations are
rejected, while unrelated changes already on disk are loaded and preserved.
Byte-identical changes do not create a copy. Radiant maintains separate
dialog-local shared-save Undo/Redo by issuing inverse expected-value writes;
these operations do not alter MAP overrides, MAP dirty state or MAP history.
To create a child-specific shared override, use:

```powershell
NODTemplates --workspace DIR --asset-root ORIGINALS --archive Extra --template CHILD --target-archive Extra.not --override-property 139 --expect "1.5" --value "2" --expect-owner BASE --expect-archive Global.not
```

The selected child must inherit the property from the expected owner/archive
and have no existing local modifier with that ID, even one shadowed by a later
basedon. The SDK inserts a verified typed native modifier at the end, leaving
ordered references, unknown editor data, parents and siblings unchanged.
All previously resolved templates and affected descendant bounds are validated
against the complete Global-first ordered database. This deliberately defers
the Document insertion API's default archive-local inheritance check: a local
definition may be shadowed by an earlier archive in the actual database.
Active archive snapshots are rechecked before committing the workspace save.

The inverse `--remove-override ID` uses `--expect` for the current child value
and `--value` for the expected revealed inherited value, with the same inherited
provenance flags. Removal requires a unique owned modifier and rejects additional
unknown payload bytes. It verifies the revealed owner/value and descendant
bounds before saving. Radiant's dialog-local history uses this inverse for
new overrides, including intentional equal-value bindings; a changed parent
blocks Undo rather than silently revealing an unexpected default.

For an existing owned override, including one with opaque unused payload bytes,
first request a read-only removal preview:

```powershell
NODTemplates --workspace DIR --asset-root ORIGINALS --archive Extra --template CHILD --target-archive Extra.not --preview-remove 139 --expect "2"
```

The detailed selected-template result describes the hypothetical inherited
defaults and includes `removal-slot`, `removal-payload` and `removal-order`.
Preview never saves or creates workspace copies. It requires a unique local
modifier with an editable inherited replacement, and validates complete-context
resolution and affected descendant bounds.

To commit, use `--remove-override ID` with the current `--expect`, revealed
inherited `--value`, `--expect-owner` and `--expect-archive`. Pass all three
preview attributes unchanged as `--slot`, `--modifier` and `--order`.
The slot is a canonical ordinal from 0 through 127; the modifier is all 36
original bytes in 72 lowercase hex characters; order is the remaining ordered
modifier IDs, each encoded as four little-endian bytes in lowercase hex.
Changed payload, ordinal, ID order or inherited provenance/value rejects the
operation without saving. The proof-less inverse above remains restricted to
payloads with no additional unknown data.

`--restore-override ID` requires the same three proof flags and inherited
provenance, with `--expect` set to the inherited value and `--value` to the
original owned value. It restores the exact bytes at the original ordinal,
not an appended/re-encoded approximation. Existing field-value edits survive;
structural changes, changed inheritance or restoration that would be shadowed
by a later basedon are rejected. These optimistic state tokens are not
authorization credentials.

Radiant exposes this workflow as **Remove template override...**, including
after reopening the dialog, with inherited-value confirmation and independent
Undo/Redo. Closing the dialog still ends its history, not the saved edits.
Arbitrary property creation, root-definition deletion, template management and
inheritance changes remain separate native-editor workflows.

With Python available, CTest's `TemplateCatalog` covers generated archives,
first-loaded precedence, inheritance failures/limits, MAP-safe names, Unicode
roots, workspace overrides, shared writes, inherited bounds, conflicts, native
byte preservation and unchanged originals. To additionally verify queries and
a workspace-only shared edit of original SC:G Global (without modifying it):

```powershell
python .\src\tools\tests\test_template_catalog.py .\build\viewer\cmake\Release\NODTemplates.exe 'C:\game'
```

### Read-only imported NSD metadata

The same headless `NODTemplates` executable also accepts:

```text
NODTemplates --workspace DIR [--asset-root DIR] --scene FILE.nsd --sources "CRC CRC CRC" --schema N
```

Radiant supplies the exact three lowercase eight-digit checksums and schema
from its unique immutable `nod_scene` MAP binding. The NSD checksum must match
the selected source byte-for-byte. Supported schemas are 1-15; only 14/15 expose
existing local MAP property editing. Scene archives come from the NSD itself,
not a guessed template or caller-supplied archive list. The normal workspace
precedence and original-asset policy apply. Queries never write source files.

Successful stdout is a pure `<nod-instance-catalog version="1">` XML document
with `sources` and `schema`, ordered `archive` entries, optional `warning` entries
and exact `instance` records. Each record contains `identity=nsd:<native offset>`,
the primary `name`, resolved runtime `type`, `problem`, `local-count` and
`effective-count`, plus separate `local` and `effective` property groups.
Property attributes extend the template-detail contract with `source=nsd|not`
and an owning-document payload `offset`; local properties have an empty archive.
Every serialized basedon reference participates in resolution, so a local
payload can be shadowed by a later inherited value. Unresolved records retain
diagnostic local metadata with editing disabled, never guessed defaults.

Edit policy reuses the native scene bridge's property classification. Protected
prefixes, including the region-class alias of tag 15, are read-only; that alias
is labelled as text rather than misrepresented as model scale. Raw, quoted and
nonfinite values and preview-dependent fields remain protected. Inherited native
values cannot be inserted as new MAP modifiers. Parser traces use stderr rather
than corrupting stdout; Radiant forwards diagnostics to its console.

Radiant's **Bind NSD source...** stores the source path in its checksummed
per-MAP editor sidecar, not in native keys. It matches exact instance offsets and
primary names, displays effective provenance and shadowing, and validates
existing local edits with ordinary editor Undo/Redo and MAP persistence.
This does not enable native modifier insertion/removal, shared NOT editing,
new placement serialization or compiler/build/export integration.

### Native NSD and NOT editors

`NSDEditor` and `NOTEditor` are optional C++17 desktop tools using the same
vendored Dear ImGui, GLFW, OpenGL, and GLEW backend as the asset viewer.
They do not require Python or Qt. GUI builds require GLFW 3.3+, GLEW, and
OpenGL development libraries; the existing viewer dependency setup can be reused.
CLI-only builds keep these dependencies optional. For example, with an existing
configured viewer build, run from the repository root in PowerShell:

```powershell
cmake -S . -B .\build\viewer -DNODSDK_BUILD_EDITORS=ON
cmake --build .\build\viewer --config Release --target NSDEditor NOTEditor native_editor_tests
.\build\viewer\cmake\Release\NSDEditor.exe 'C:\game\Levels\level.nsd'
.\build\viewer\cmake\Release\NOTEditor.exe 'C:\game\Misc\Templates\Global.not'
```

For a new build, pass your existing dependency toolchain/prefix to CMake as
with the viewer. Editors can also be enabled when configuring `src/tools`
standalone, or with `NODSDK_BUILD_VIEWER=OFF`; building the asset viewer is not
required. Root builds use `cmake/Release`; standalone tools builds place these
editors and their test executable under `editors/Release` with Visual Studio.
Single-configuration generators omit the `Release` directory.

For protected authoring, pass `--workspace DIR` and optionally `--asset-root DIR`
before or after the input path:

```powershell
.\build\viewer\cmake\Release\NSDEditor.exe --workspace 'C:\SCGhostMapping\nod' --asset-root 'C:\game' 'C:\game\Levels\level.nsd'
```

In this mode **Save workspace copy** proposes a destination preserving the
source's asset-relative path. All saves, including explicit overwrite and
subsequent Open/drop documents, are restricted to the workspace. Original
assets remain read-only. Overlapping workspace/asset roots, traversal and
resolved symlink escapes are rejected. Failed workspace configuration disables
saving instead of reverting to unrestricted behavior. Missing destination
directories are created inside the workspace. Without `--workspace`, the
standalone editors retain their normal save/overwrite behavior.

NSD contexts first look for template archives in the workspace, then use the
original asset root only for absent archives. Malformed, ambiguous or unreadable
workspace archives are errors, not reasons to fall back. Context is a snapshot;
reopen the scene after modifying archive copies. These options control file
access, not build/export functionality. Supported NOT creation and NOT/NSD property
insertion/removal are described below; NSD placement creation is not supported.

Open a file using its command-line path, **Open**, or drag and drop. On Windows,
**Open** uses the system file picker with NSD/NOT filters and Unicode paths;
other platforms currently retain the path-entry popup. Canceling or selecting an
invalid file preserves the current document. Load errors can be dismissed with
**OK**, **Enter**, or **Escape**; afterward another file can be selected normally,
without a stale error popup blocking the editor.
**Ctrl+N** creates a NOT archive. **Ctrl+O**, **Ctrl+S**, **Ctrl+Shift+S**, **Ctrl+Z**, and **Ctrl+Y** support
open, save, save-as, undo, and redo when no property input is active.
Select a tree item and property, edit its input, and click **Apply**.
The input is not applied automatically. The **Changes** tab shows changed bytes
since load/save; **Diagnostics** explains partial decoding and inheritance issues.
The paged, read-only raw view covers either the selected record or the entire file.

- **NSD:** placements grouped by exact native template name; verified position and
  native Euler rotation components, existing sector indices, and existing tag-4
  model overrides are editable. Additional validated modifiers use the shared
  engine metadata. Template references use the dedicated guarded reassignment
  operation below, not ordinary field or raw-payload editing.
  Sector/model existence and NIL/NPD cross-file consistency are not validated
  or rebuilt.
- **NSD placement overrides:** in **Effective properties**, **Make local
  override** copies the inherited property's complete 32-byte payload from its
  owning archive snapshot into the selected counted placement. It appends after
  every existing modifier, so the new value wins even over a late `basedon`;
  earlier local values/references are preserved. Archive provenance is explicit,
  including normalized aliases and first-loaded duplicate archive handling.
  The source NOT file is not changed. Edit the selected new field in
  **Properties**; known types use typed values, while raw payload editing
  requires the explicit unknown-semantics checkbox.
  **Remove local property** requires confirmation and removes only that
  modifier, allowing an inherited or earlier local value to become effective.
  Removal does not require resolved template context.
  Both operations require a verified counted array and enforce the native
  128-modifier limit. Class references, the primary rotation/position/sector
  and region/script prefix are not removable. Inherited insertion of references,
  transforms or sector (IDs 0–3) is not supported; use the existing field
  controls where applicable. New values that reinterpret a prefix or change
  placement recognition/boundaries are rejected atomically. Uncounted legacy
  fields remain fixed-width edits only. Full payload tails, subsequent placement
  bytes, frames/unknown data and declared archive lists are preserved. Effective
  values, selected fields and offsets refresh; mixed field/override/archive
  Undo/Redo captures the original archive snapshots without rereading disk.
  Native workspace Save/Save As and reopen retain local/inherited provenance.
  Placement creation/deletion and project-wide reference rewriting are not
  enabled by these controls.
- **New NSD local properties:** select a placement, then **Properties > Add
  local property**. The searchable picker uses the same verified ASCII,
  float32, uint32 and vector3 metadata/serializer as NOT authoring, including
  properties absent from the template. New payloads are zero-initialized outside
  the typed value. Printable ASCII is limited to 31 bytes; float components must
  be finite and integer input must fit uint32. Enums, flags, resource existence
  and property applicability to the selected gameplay class are not validated.
  References, primary transforms and sector (IDs 0–3) are excluded; unknown/raw
  native storage cannot be inserted. Values append after all existing modifiers,
  so repeated IDs and late inheritance retain their order while the new value
  wins. Removing it restores the earlier local or inherited value.
  The same counted-array, overlap, prefix and placement-recognition guards
  apply. Insertion enforces both the native 128-modifier limit and expanded
  limits when template resolution can reach them. Missing/cyclic dependencies
  or absent context remain explicit Diagnostics; effective lists are not
  presented as partially resolved and runtime validity is not claimed.
  Existing source archives and opaque scene data remain unchanged. Mixed
  field/insertion/removal/archive history and workspace Save/Save As/reopen
  retain snapshots and exact unrelated bytes.
- **Change placement template:** the NSD details header opens a searchable
  picker showing cached template names and source archives. Global loads first;
  duplicate names show only the winning definition. Choose a target and confirm
  that inherited values may change before applying. Only the zero-padded
  32-byte primary `basedon` name is replaced; local overrides, secondary
  references, transforms, sector/region prefixes, modifier counts and opaque
  frames/geometry remain byte-identical. The tree and effective values refresh.
  Complete old and candidate modifier expansions must both resolve, fit the
  expanded limit and contain the same uint32 native type (property 20), matching
  the runtime's class-selection stage. Winning local/late-inherited type values
  participate in this comparison. Missing/cyclic dependencies, absent type
  metadata, unrepresentable names, uncounted/overlapping arrays and decoded
  layout changes are rejected atomically. Cross-class body conversion is not
  implemented. This compatibility check does not validate gameplay enums,
  resources or runtime behavior. The picker is disabled without cached context;
  reopen the scene to refresh disk changes. Snapshot-aware mixed Undo/Redo and
  workspace Save/Save As/reopen are supported without editing any source NOT.
- **NSD secondary inheritance:** **Inheritance order** shows every native
  modifier, including the protected primary reference, rotation, position and
  canonical sector/region prefix. Select an unprotected row and use **Insert
  secondary reference** to insert before it; select a protected row or no row
  to append. The same cached template picker requires confirmation. Select a
  secondary `basedon` for confirmed **Change reference** or **Remove reference**,
  or move it earlier/later across local modifiers. Complete 36-byte moves retain
  hidden payload bytes; replacement changes only the new name and terminator,
  retaining bytes after it. Repeated references are never deduplicated.
  Later values win, so moving a reference can shadow or restore a local value
  without deleting that value.
  All operations require verified nonoverlapping counted arrays, cached
  context, fully resolved old/new expansions and the same runtime type
  (property 20). Native and expanded limits apply. References cannot move into
  the required prefix; changes that promote a script/sector into that prefix
  (even an empty script) or create false placement recognition are rejected.
  Unknown/cyclic old graphs cannot be repaired here by guessing the old class;
  fix template context and reopen first. Missing context/legacy layouts still
  permit read-only diagnostics, not unchecked structural mutations.
  Counts, later placement offsets and effective values refresh while frames,
  geometry and unrelated bytes remain intact. Mixed field/reference/archive
  Undo/Redo retains snapshots without reading disk, including no-ops, saved
  baselines and redo branching. Save to a workspace copy; source NOT/NSD files
  are not rewritten.
- **NOT:** local modifiers, separately preserved read-only editor data, and an
  effective-property tab. That tab expands every `basedon` reference in source
  order, case-insensitively; the last applied value wins, even when supplied by
  a late reference. The tree shows only the first reference for navigation.
  Multiple references, missing dependencies, cycles, and trailing bytes produce
  diagnostics. Failed expansion does not expose a partial effective list.
- **New NOT archives:** **New archive** (or **Ctrl+N**) asks for an initial
  root name using the same 1–31 printable-ASCII-byte validation as other
  template operations. The archive starts with one empty root; zero-template
  archives remain unsupported. No type, flags, model or other gameplay defaults
  are invented. An empty root cannot instantiate in the runtime: add its
  required type and other appropriate properties before runtime use.
  Creation writes nothing to disk, resets the previous path/history/template
  context, retains the workspace policy and starts modified. New/Open/Close
  are protected by the unsaved-change prompt; canceling creation or entering an
  invalid name leaves the previous document intact. Creation is a document
  boundary, not an undo step; subsequent edits use normal Undo/Redo.
  First Save opens Save As, suggesting
  `<workspace>/Misc/Templates/Untitled.not` (or `Untitled.not` in standalone
  mode). Existing destinations still require overwrite confirmation and
  originals remain protected when workspace policy is enabled.
- **NOT new local properties:** in **Properties**, **Add local property**
  provides a searchable list of known modifiers with verified ASCII, float32,
  uint32 or vector3 storage. Enter an explicit value and use **Append property**.
  New payloads are zero-initialized outside their value; they are appended
  after all existing modifiers, preserving any earlier values of the same ID.
  Thus an appended local value wins even over a late `basedon`. Enum/flag
  values remain raw decimal uint32 bits, not validated gameplay choices.
  `basedon` uses the dedicated inheritance controls; unknown IDs and unusual
  raw storage types cannot be inserted. Invalid values, more than 128 local
  modifiers, local cycles and more than 128 known expanded IDs are rejected,
  including overflow in affected descendants. Missing external dependencies
  remain diagnosed; their full graphs/counts are not validated here.
  Counts/TOC offsets, effective values and selected input refresh. Insertion is
  one structural undo step and preserves unrelated payloads/editor data/tails;
  native workspace save/reopen and mixed field/template history are supported.
- **NOT local overrides:** in **Effective properties**, **Make local override**
  appends a complete copy of the inherited 32-byte payload after all existing
  modifiers. This ensures it wins even when a late `basedon` shadows an earlier
  local modifier of the same ID; that earlier modifier is preserved. Edit the
  selected new field in **Local properties**. **Remove local property** asks for
  confirmation and deletes only the selected modifier; effective inheritance
  is refreshed immediately. Names, `basedon` references and separate editor
  metadata cannot be removed through this control. These operations update the
  template's modifier count and later archive offsets, preserve unrelated bytes including
  editor data/trailing bytes, enforce the 128-modifier limit, and support
  Undo/Redo and native save/reopen. Counted NSD placement overrides use the
  separate behavior described above.
- **NOT templates:** select a template in the tree and use **New child** to
  create a minimal template containing a `basedon` reference to it, or
  **Duplicate** to copy its complete local record, including editor metadata
  and trailing bytes, under a new name. Existing inheritance references are
  retained by duplication, not redirected to the copied source. Names require
  1 through 31 printable ASCII bytes and must be unique within the open archive
  (case-insensitively). Cross-archive name collisions are not checked; normal
  first-loaded-name precedence still applies.
  **Delete template** checks every `basedon` in the open archive, including
  late references. Referenced templates, archives containing unverifiable
  references and deletion of the final template are rejected. Other archives
  and NSD scenes are not scanned or rewritten; the dialog requires explicitly
  acknowledging that those external references have been checked.
  Creation/duplication/deletion update the native TOC and template count while
  retaining unrelated record bytes and pre-data padding. The tree, selection
  and effective properties refresh after changes and Undo/Redo. The native
  2048-template limit is enforced. Use normal workspace Save/Save As to persist
  the archive; original assets remain protected.
- **NOT rename/inheritance:** **Rename** changes the selected name and every
  matching local `basedon` string, case-insensitively, preserving payload bytes
  after the new terminators. It rejects duplicate names, unverifiable local
  reference payloads and names already referenced as another missing/external
  dependency, rather than accidentally retargeting those references. Like
  deletion, rename requires acknowledging that references outside the open
  archive have been checked; those files are not rewritten.
  **Reparent** replaces the explicitly selected `basedon` slot, preserving all
  other references and their source order. If the template has no `basedon`,
  it inserts a first reference before local modifiers, so those locals retain
  normal override precedence. Choose the new parent from the open archive.
  The changed template, its reachable local dependencies and affected local
  descendants are checked for cycles, invalid reference payloads and more than
  128 known expanded property IDs. Every local reference is examined, not only
  the first tree parent, even when another dependency is missing.
  Missing external dependencies remain diagnosed and do not expose partial
  effective-property lists; their full graphs, cycles and expanded counts
  cannot be verified without cross-archive context. Tree/effective values
  refresh after rename/reparent and Undo/Redo, and native save/reopen is
  supported. Exact no-ops do not add history.
- **NOT inheritance order:** the **Inheritance order** tab lists every native
  modifier in serialized order, including both `basedon` references and local
  values. **Insert base reference** selects a parent from the open archive and
  an explicit position before any native modifier or at the end. Existing and
  repeated references are retained, not deduplicated. Insertion uses the same
  local-cycle/expanded-property validation as reparenting.
  Select a reference and use **Move earlier / Move later** to move its complete
  36-byte modifier across the adjacent reference or local value. Other
  modifiers keep their relative order; crossing a local value can change which
  value wins. **Remove reference** requires confirmation and deletes only that
  selected reference, including the final parent if requested. Removal can
  repair invalid/cyclic references incrementally. Removal/reordering does not
  add dependency edges; existing diagnostics are retained/refreshed rather
  than preventing repair. Names and separate editor metadata are never
  insertion/movement destinations. Unexposed payload bytes, metadata and
  trailing data are preserved. The tree and effective values refresh, and
  all these operations support mixed Undo/Redo and workspace save/reopen.
- **Shared property schema:** all 177 modifier IDs/names, handler classes, and
  storage types are recovered from the Ghost engine table. Valid ASCII strings,
  float32 values, integer bits, and three-component vectors have typed inputs.
  Integer signedness/flags/enums and gameplay semantics are not fully validated.
  Unusual/unknown types, including `PARTICLENAME` (ID 68), remain raw. Enable
  **raw property payload editing** to supply exactly the displayed number of
  two-digit hex bytes. Raw editing preserves layout, not gameplay correctness.
- Fixed-width model strings accept at most 31 printable ASCII bytes. Bytes beyond
  the new terminator are preserved. Non-finite transforms, integer overflow,
  invalid payload lengths, and field edits that change the decoded layout are
  rejected. Explicit native property insertion/override/removal operations
  rebuild and validate the changed document layout instead of bypassing this
  field-edit guard.
- Malformed/truncated NOT modifier arrays or editor-data blocks are rejected;
  valid editor data and additional trailing bytes are preserved.
  Zero-template archives, cross-archive reference rewriting,
  NSD placement creation/deletion, insertion of unknown/raw native
  types, resizing individual payloads,
  cross-runtime-class conversion, complete
  gameplay validation and structural native level recompilation are not
  implemented.

The viewer and scene-preview tools now obtain template models from the actual
NOT database rather than a hard-coded entity resolver/type catalog. For validated
counted NSD arrays, the entire placement array is expanded through the same
resolver: a late `basedon` can replace a local model, repeated local model IDs
use the final value, and an empty model remains an explicit empty value.
Dependencies are evaluated even if a local model is present. Uncounted legacy
placements retain the historical explicit-model/template fallback with a warning;
they do not expose a partial effective-property list.

The NSD **Effective properties** tab shows local and inherited values with source
provenance. Opening `Levels/level.nsd` automatically loads the read-only
`Misc/Templates/Global.not` snapshot followed by the NSD's declared archive list.
The variable-length list shifts the object count/placement offsets; it is not
part of the first placement. Up to nine names are supported, including
extensionless and nested relative names, with case-insensitive file lookup.
Templates resolve across the complete ordered archive set. Duplicate template
names retain their **first-loaded definition**, not the last; Diagnostics names
the shadowed/winning archive. Effective values show both archive and template
provenance.

In the NSD editor, **Template files** opens a scene-wide archive-list panel,
including when there are no placements. **Add archive at end** accepts a
reference relative to `Misc/Templates`: 1 through 31 printable ASCII bytes,
with optional `.not` extension and supported nested relative paths.
New entries cannot duplicate an existing canonical archive name or the
implicit `Global.not`. Existing repeated/explicit-Global entries in legacy
scenes are preserved rather than silently deduplicated.
Select a row to **Move archive earlier/later**, or **Remove selected archive**
and explicitly confirm removal. Complete 32-byte slots, including hidden bytes
after their terminators, move intact. Global remains loaded first regardless
of the declared order. The nine-declared-archive limit is enforced.

List changes update only the native archive count/list; the complete remaining
scene body, placements and unknown sections are retained byte-for-byte.
Existing listed archives reuse their snapshots; newly needed archives are read
from the workspace, falling back to original assets only on true absence.
A missing/malformed newly needed archive rejects the operation atomically. Neither list
editing nor removal writes/deletes archive files.
Effective values and duplicate-definition/dependency diagnostics refresh
immediately. Missing references, cycles and expanded-property failures reachable
from verified placements are diagnosed without exposing partial effective
lists; such semantic problems do not prevent list editing/repair. Unused
templates, gameplay applicability and project-wide references are not fully
validated, and references are not automatically rewritten.

Close the panel to use normal Undo/Redo. Each list edit restores both serialized
bytes and the associated archive snapshots, without rereading disk; mixed
field/list history and native workspace Save/Save As are supported. Exact
no-op moves and failed edits do not add history or discard redo branches.
Re-adding a removed archive reads its current file; reopen the scene to refresh
all loaded snapshots. The UI remains subject to normal unsaved-change guards.

Without any template context and with no declared scene archives, local editing
remains available and Diagnostics explains why effective values are unavailable.
A missing/malformed declared archive or invalid existing Global produces a load
error without replacing the current document. Context replacement is atomic,
including buffer-based contexts that must supply every declared archive.
Reopen the NSD to refresh all snapshots. External values cannot be edited in
their source archives from the NSD document; use Make local override for a
placement-local copy. The standalone NOT editor continues to edit one archive at a
time. These tools use extracted files, not NOB mounting or live engine archive
unloading. Static previews now apply uniform `MODELSCALE` (ID 143, after model
creation) and world-space `MODELOFFSET` (ID 175, gated by renderflags ID 12 bit
`0x2000`). Schema-12/13 previews preserve native entity origins and carry immutable
`nod_modelscale`/`nod_modeloffset` for Radiant; fallback brushes bake the same
transform. No `_offset` name heuristic or LOD-scale geometry multiplier is used.
Zero scale produces a diagnosed point rather than degenerate geometry.
Older sidecars retain their original contracts; re-enrich to adopt these
transforms. Expansion does not simulate every runtime property handler.
The legacy `script_name`/MAP `nod_script` fields retain their archive-compatible
meaning; the property editors correctly identify ID 15 as `script` and ID 161
as `EFFECT1`. See the [format reference](../../docs/FORMAT_REFERENCE.md).

Use **Save As** for the first edit of an original game asset. No-edit saves are
byte-identical. Fixed-width edits patch only their exposed payload ranges;
explicit structural actions also update native counts, records and offsets.
Saves validate
the resulting decoded layout, write/flush a sibling temporary file, then publish
it atomically. Existing Save As destinations require confirmation. A changed or
deleted current file is not overwritten: reload it or choose another destination.
Unsaved applied changes require an explicit discard decision before creating
or opening a different file or closing.

Both tools accept `--check <file>` for headless indexing/diagnostics and
`--smoke-test <file>` to load and render three frames without saving.
The NOT smoke test selects **Inheritance order**; the NSD test opens
**Template files** and renders **Effective properties**, the complete
**Inheritance order** view and the **Add local property** dialog when placements
exist. With cached context it also renders the placement-inheritance picker.
Neither dialog applies changes during smoke.
Each fails if its required views were not rendered.
This checks GUI startup/rendering, not interactive button clicks;
document regressions separately exercise the mutations and native persistence.
The local document regression suite does not need GUI libraries. After
configuring the separate regression build above:

```powershell
ctest --test-dir .\build\local-regressions -C Release -R NativeEditorDocuments --output-on-failure
.\build\local-regressions\Release\native_editor_tests.exe 'C:\game\Misc\Templates\Global.not' 'C:\game\Levels\level.nsd'
```

Optional test arguments validate real assets using disposable copies, including
byte-identical no-edit saves, localized edits, undo/redo, and disk-change conflicts.
They do not modify the supplied assets.
The generated-fixture suite also checks NOT inherited override insertion/removal,
exact count/TOC updates and unrelated-byte preservation, full payload copying,
late-reference shadowing, empty modifier arrays, the 128-modifier limit,
mixed structural/field Undo/Redo, redo branching and workspace save/reopen.
Template-management fixtures cover derived creation, exact duplication,
first/middle/last deletion, preserved metadata/trailing bytes/pre-data padding,
local case-insensitive and late-reference protection, invalid reference
rejection, unique-name/length validation, the 2048-template limit and rejection
of final-template deletion and NOT operations on NSD documents.
Rename/reparent fixtures cover exact changed-byte ranges, late/case-insensitive
reference rewrites, no-ops, name-capture prevention, invalid-reference repair,
secondary-reference ordering, first-parent insertion, local cycles masked by
missing dependencies, existing-cycle repair, expanded-ID limits including
affected descendants, duplicate-ID counting, mixed history and protected
native save/reopen.
Ordered-inheritance fixtures cover explicit insertion positions, repeated
references, moves across local values, primary-parent refresh, exact
payload/TOC preservation (including unexposed typed-field bytes), final-parent
removal, incremental invalid/cycle repair, unknown modifier classification,
no-ops, limits, mixed history, original protection and native save/reopen.
Archive-authoring fixtures cover exact minimal native creation, invalid-name
preservation of the previous document, unsaved identity/history boundaries,
retained workspace protection, all four supported storage types and invalid
values, exact count/TOC/payload preservation, repeated-ID precedence, local and
descendant-expanded limits, mixed undo/redo across save, overwrite confirmation,
native save/reopen and NSD consumption of the authored workspace archive.
Scene archive-list fixtures cover exact inserted/removed/moved name slots,
unknown scene-body and hidden-slot bytes, implicit-Global/first-loaded
precedence, workspace overrides and malformed-override rejection, legacy
duplicates, no-ops, invalid names, the native nine-slot limit, empty scenes,
cross-archive cycle diagnostics, mixed field/list history with files removed
from disk, redo branching, original protection and native save/reopen. Optional
real-scene tests add/remove a disposable workspace archive and verify complete
source-byte recovery after save/reopen.
Placement-override fixtures cover full inherited float/uint32/vector/string/raw
payload copies from the correct archive, preserved typed-field tails, primary
prefix protection, three-modifier minimum and 128-modifier maximum, first/last
placement offsets, late-reference precedence, local removal without context,
legacy/overlapping arrays and false placement recognition, mixed field/archive
history after removing source files, redo branching and protected native
save/reopen. Optional real-scene tests verify an actual inherited override,
removal, full source-byte recovery and workspace save/reopen.
Typed NSD insertion fixtures cover all four storage types with exact serialized
payload bytes, zero-filled tails, empty/31-byte strings, invalid/protected/raw
inputs, duplicate-ID fallback, late-reference precedence, first/last placements,
context-free insertion/removal, empty/legacy/overlapping arrays, false placement
recognition and protected prefixes. Slot 128 succeeds and slot 129 is rejected;
a new ID cannot exceed 128 resolved values while a replacement at that limit
succeeds. Mixed field/structural history survives deleted source archives,
failed operations preserve redo, new edits branch correctly and workspace
save/reopen leaves originals unchanged. Optional real-scene tests also insert a
typed local value, verify exact bytes, undo/redo, save/reopen, then remove it and
recover the original scene byte-for-byte.
Template-reassignment fixtures verify first-loaded/canonical archive choices,
case-insensitive target lookup, exact primary-name-only changes, inherited-value
refresh, retained local overrides, three-modifier layouts, mixed history after
source removal, no-op/failed/new redo branches and protected save/reopen.
Missing context, wrong document/index, legacy/overlapping arrays, untyped old or
new expansions, missing/cyclic dependencies, cross-class changes and expanded
overflow are rejected. Exactly 128 resolved values are allowed; winning local
type overrides are respected. Optional real-scene tests add a workspace-derived
target, reassign/save/reopen, restore the old reference and remove the added list
slot to recover the original complete scene bytes.
Secondary-inheritance fixtures check exact ordered insertion/replacement/
movement/removal across local values and first/later placements, repeated
references, retained payload tails, complete prefix rows, 3/128 native counts,
slot 128 insertion versus slot 129 rejection, exactly 128 expanded values and
overflow rejection. Runtime-class changes through removal/replacement/movement,
missing/cyclic old/new dependencies, protected-prefix promotion (including
empty scripts), legacy/overlapping arrays and false placement recognition are
rejected atomically. Mixed field/reference/archive history survives missing
files and preserves no-ops, saved baselines and redo branches. Optional
real-scene tests insert/remove a secondary reference with exact body/count
bytes, undo/redo and protected workspace save/reopen.

### NILtoMAP

```sh
NILtoMAP input.nil output.map
NILtoMAP --scale 32 --thickness 2 input.nil output.map
NILtoMAP --help
```

- `--scale`: positive position multiplier, default `1`.
- `--thickness`: positive extrusion depth in output MAP units, default `1`.
- Default output uses brush primitives and preserves the original normalized
  surface UVs, including shear, mirroring, and constant coordinates.
  `--classic` selects historical classic MAP syntax with default texture alignment.
- `--` ends option parsing, allowing filenames beginning with `-`.
- Coordinates remain NIL world-space XYZ, Z-up. Sector centers are not added.
- Each nondegenerate triangle becomes a five-plane convex triangular prism,
  extruded opposite its winding normal. Strip parity, degenerate connectors,
  restart index `0xffff`, and triangle lists are handled.
- The front face uses the NIL material token; back/side faces use `common/caulk`.
  Backslashes become slashes and a leading `textures/` is removed, as Quake III
  MAP texture names are relative to that directory. Supply matching shaders and
  textures in your GtkRadiant game project; materials are not extracted.
- Successful conversion reports brush and skipped-degenerate counts.
  Invalid input, unsupported layouts, and I/O errors return a nonzero exit code.
  The output is not opened until parsing and temporary MAP generation succeed.
  An output write failure can leave a partial file.

Open the result in GtkRadiant with a Quake III game configuration. Very large
levels may produce many brushes; this tool does not merge coplanar triangles.

## Supported layout and limitations

The base layout follows the repository's C++ NIL parser: magic `NIL\x10`, version 35,
**32-byte** shader/environment/reference names, four bytes for vertex count plus
padding, 36-byte vertices, optional TBN streams, and 47-byte groups.
When `hasGeo == 2`, the subsequent type-1 stream contains a second vertex/TBN
pool (36-byte vertices followed by 36-byte TBNs). Bumped groups (`vertType == 3`)
use that pool unless the primary pool already has TBNs. This corrects the
older "lightmap patches" interpretation, verified against the supplied Hive
levels' group indices and bounds. Unsupported secondary stream types fail
explicitly. Some format notes and the Blender importer
describe 33-byte names; that alternative layout is not supported by this tool.
All consumed records and surface index ranges are bounds-checked.

This is **approximate mesh reconstruction**, not recovery of the original MAP:

- Original brush topology, CSG, and entity definitions cannot be inferred.
  Output contains only a `worldspawn`; NIL things, surface things, portals,
  lightmaps, and environment references are consumed but not exported.
- Baked lighting, vertex colors, and vertex normals are not preserved.
  Front-face UVs are preserved unless `--classic` is selected.
  Near-degenerate slivers or extreme UV gradients can exceed the MAP/editor's
  binary32 matrix precision; the exporter warns when roundoff exceeds 0.001 UV.
- Undecoded post-sector collision/BSP/lightmap data is ignored with a warning.
- Thin triangle brushes can overlap or leave gaps. A sealed, playable level
  requires manual cleanup, appropriate scale/depth, and gameplay entities.
  This tool does not claim compile-ready reconstruction.
  Extrusions that collapse at MAP float precision are rejected explicitly.
- Plane triples below GtkRadiant's binary32 normal tolerance are represented by
  well-separated coplanar points instead of the original tiny edge vectors.
  This preserves the oriented plane rather than enlarging the source triangle.
  Current SCGhost GtkRadiant builds recognize the native UV worldspawn marker,
  preserve fractional planes, and skip tiny-brush cleanup. On a fresh map load
  they select brush-primitive mode automatically, avoiding lossy classic texture
  conversion. Older builds require brush-primitive mode, **Don't clamp plane
  points** (`NoClamp=true`), and **Clean tiny brushes** disabled.

Tests generate synthetic NIL files and check brush plane orientation, convexity,
depth, shader tokens, strip/list handling, optional sections, every truncation
of a complex sector, malformed data, and CLI errors. No game assets are bundled.

## Scene round-trip foundation

The same build also produces `NODscene`, which preserves complete NIL/NSD/NPD
sources in a sidecar and compiles a restricted set of native entity/navigation
edits back into those sources. See [the scene bridge contract](SCENE_BRIDGE.md)
for supported fields, custom Radiant entity definitions, and validation gates.
`NODscene preview` enriches an exported scene using verified NOT/NSD model
references and exact NOD/NMB lookup. It emits native model paths and `_frame`
member indexes for the fork's **NOD Authoring** profile, and one
native per-type entity per resolved NSD placement; compilation restores native
record edits without requiring the original model assets. NIL static geometry
becomes a `func_group` with `nod_id=nil:world`: each nondegenerate triangle is
extruded by 0.01 native units, with its material on the front and caulk elsewhere.
Any generated OBJ entity preview is converted the same way, with native placement
and rotation baked into the brush coordinates. Native NOD/NMB models remain
native `nod_nsd_type_<encoded type>` entities. Ordinary source exports use schema
10; transformed OBJ/native previews use 12/13. Opt-in `export-properties` exposes
existing typed local NSD modifiers in schema 14, retained as schema 15 by native
enrichment. Known local string/float/vector/uint32 payloads can be safely patched;
inherited-only values require NOT editing. Use `entitydefs` with `--assets-root`
and `--inventory` to extract observed local/effective properties and provenance
across all supplied NSDs. Versions 1-13 remain compilable with their original
contracts. NSD OBJ fallbacks retain their native point entity and link
to a separate editable brush group instead of replacing the entity with geometry.
Standalone NODs replace bundle members
only when their rendered geometry/materials/colors match; otherwise the
original NMB member remains authoritative. Units stay at scale 1.
Native material paths are `textures/nod/<shader>`; the original read-only
`Materials` and `3D/Materials` tree is mounted by the authoring profile.
No prop OBJ, MTL, or converted textures are generated. Only NIL static geometry
needs an intermediate generated OBJ/MTL, since the editor has no NIL model loader.
New fallback MAP entities reference no OBJ; the sidecar archives their brushes.
`NODscene preview-obj` uses generated-diffuse materials and converts all resolved
meshes into brush groups. Current GtkRadiant builds protect identified NOD groups
from plane-point clamping and tiny-brush cleanup and use thin-brush clipping
tolerance. Older builds require **Don't clamp plane points** enabled and
**Clean tiny brushes** disabled. They are editable and
saveable in MAP, but native compilation rejects geometry/material/group-placement
edits before opening outputs; unchanged scenes still round-trip byte-for-byte.
Brush primitives retain source mesh UVs, including transformed entity fallbacks.
The editor preserves valid zero/constant mappings and writes round-trip-safe
float precision when saving. Existing classic sidecars remain supported; re-export
older previews to obtain UV-preserving brushes. Native compilation also rejects
UV edits until geometry compilation is implemented.
They do not reconstruct original CSG or guarantee a sealed/playable BSP. Native animation,
geometry compilation, and game behavior validation remain separate
milestones.

Generate editor palette definitions from observed native types with
`NODscene entitydefs nod_types.def level.nsd [other-level.nsd ...]`.
Load these alongside the supplied `entities.def` in the NOD Authoring profile.
The supplied catalog contains 193 observed types, with no inferred Quake gameplay
mapping. It prepares native type identity and model visualization for future
from-scratch authoring; it does not supply undecoded engine defaults or enable
creation/deletion until native record and geometry serializers are implemented.
