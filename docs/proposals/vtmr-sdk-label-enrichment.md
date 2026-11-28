# Proposal: Data Label Enrichment from the VtMR NOD Engine SDK

Status: Draft proposal. Nothing is implemented.

## 1. Summary

The official NOD Engine SDK v1.1 was released by Nihilistic Software in 2000
for *Vampire: The Masquerade – Redemption* (VtMR). StarCraft: Ghost runs on a
later version of the same engine.

Most SDK binary layouts do not match Ghost. Many **names and meanings** do.
This proposal uses them as labels only. It never uses them to change parsing.

Four changes are proposed:

1. Label NAD keyframe tags in the viewer inspector and in NODExport JSON.
2. Add property tooltips in the NSD and NOT editors, matched by name.
3. Check NMS motion modes and flags in NODSurvey.
4. Optional, lowest confidence: label enum values and flag bits (section 7a).

Every label from the SDK is marked as **"VtMR name, unverified for Ghost"**.

### Confidence labels used in this document

| Label | Meaning |
|---|---|
| **Fact** | Stated in an SDK file or in this repo's code. Path given. |
| **Measured** | Computed by comparing SDK data with repo code (Appendix B). |
| **Suggested** | A proposal or an inference. Not confirmed. |
| **Ambiguous** | Sources conflict or are unclear. Needs Ghost data. |

Unmarked rows in the tables are **Fact**.

## 2. Source

| Item | Value |
|---|---|
| Title | NOD Engine SDK v1.1 |
| Publisher | Nihilistic Software, Inc., 2000 |
| Game | Vampire: The Masquerade – Redemption (Activision, 2000) |
| Archive | `NODSDK_v1_1.zip`, SHA-256 `3c4a26212b3df49c05ee4df96ae0ce392ad555c01f7dbe77c0a812fd6f28f5a3` |
| Installer | `NODSDK.exe` (InstallShield), SHA-256 `32711edf0137faf4f012d419240e6dd1146ac1919a42b363a8e9662a9b22b7c4` |
| Extraction | `7z` for the self-extractor, then `unshield` for `data1.cab` |

Files cited below (paths inside the installer cabinet):

| Short name | Path | Content |
|---|---|---|
| `[SDK-README]` | `Documentation/NodSDK/readme_11.txt` | Release notes and tool list |
| `[SDK-LICENSE]` | `_Support_Language_Independent_OS_Independent_Files/license.txt` | SDK license |
| `[SDK-NOD]` | `Documentation/NodSDK/Docs/nod.htm` | NOD model spec, version 7 |
| `[SDK-NAD]` | `Documentation/NodSDK/Docs/nad.htm` | NAD animation spec, version 3 |
| `[SDK-NMS]` | `Documentation/NodSDK/Docs/nms.htm` | NMS motion set spec |
| `[SDK-NIL]` | `Documentation/NodSDK/Docs/nil.htm` | NIL level spec, version 27 (partial) |
| `[SDK-NPD]` | `Documentation/NodSDK/Docs/npd.htm` | NPD **particle** spec |
| `[SDK-TAGS]` | `NOTEditor/NodSDK/noteditortags.txt` | 224 NOT template tag definitions |
| `[SDK-DATA]` | `NOTEditor/NodSDK/noteditordata.txt` | NOT editor enums and file extensions |
| `[SDK-ENT]` | `Embrace/NodSDK/entities.def` | Embrace (QERadiant) entity definitions |

Note: the package is v1.1, but each spec page is headed "NOD Engine SDK 1.0".
So this document cites "NOD SDK 1.0/1.1".

## 3. License constraints

`[SDK-LICENSE]` grants personal, non-commercial use with a licensed copy of
VtMR. It also forbids:

- copying the Toolkit or the materials that come with it, except for backup
  or use with the game;
- reverse engineering, modifying or decompiling the Toolkit.

Rules for this proposal:

- **Do not** vendor SDK files, HTML, binaries or verbatim description text.
- **Do** store facts needed for interoperability: tag names, numeric values
  and data type classes.
- **Do** write all tooltip text in our own words.
- **Do** cite the source next to each label: `source: NOD SDK 1.0/1.1 (2000), VtMR`.

This is a project policy, not legal advice. A maintainer should approve it
before merge.

## 4. Research findings

### 4.1 Format comparison

The VtMR and repo columns are **Fact**. The last column is **Suggested**.

| Format | VtMR SDK | This repo (Ghost) | Can labels be reused? |
|---|---|---|---|
| NOD | v7, 32-char material names, at most 64 bones, each vertex on one bone plus an optional parent weight `[SDK-NOD]` | v10+ layout with vertex sets and LOD strips (`src/parsers/NODParser.h:53`) | No. The layouts differ. |
| NAD header | version, num tracks, flags, duration `[SDK-NAD]` | Same order, version 4 (`src/parsers/NADParser.cpp:53-58`) | Yes. |
| NAD track | NumKeys, BoneNum, TrackType (0 rot, 1 trans, 2 scale) `[SDK-NAD]` | Same order (`NADParser.cpp:79-81`) | Yes. Type 2 is unknown in Ghost. |
| NAD key | 56 bytes: Frame, FrameScale, Value, CFactor ("constant"), BFactor ("linear"), AFactor ("square") `[SDK-NAD]` | 52 bytes: frame, value, `b` (linear), `c` (quadratic), `d` (cubic) (`NADParser.h:45-51`, `NADParser.cpp:84-106`) | **Ambiguous.** Both have 3 vec3 factors after the value. The names do not line up: VtMR says constant/linear/square, Ghost says linear/quadratic/cubic. Ghost stores no FrameScale on disk. Do not relabel the coefficients from the SDK. |
| NAD tag | {FrameNum float, TagType uint} `[SDK-NAD]` | {tag_type uint, frame_num float} (`NADParser.cpp:119-120`) | Yes for names. Field order differs. |
| NMS | Text: motion mode lines, `tag` lines, `mouth` lines `[SDK-NMS]` | `src/parsers/NMSParser.h` keeps raw strings | Yes. |
| NOT tags | 224 named tags with type and category `[SDK-TAGS]` | 177-entry engine table (`src/parsers/NODProperties.cpp:30`) | Yes, **by name only**. |
| NPD | Particle emitter text format `[SDK-NPD]` | Binary path data (`src/parsers/NPDParser.h`) | **No.** Same extension, different format. |
| NIL | v27, sectors, planes, BSP nodes `[SDK-NIL]` | Different version | Background only. |

### 4.1a Other SDK specs checked

| Spec | Repo status | Use |
|---|---|---|
| NCO (conversations) | `NCOParser.h` reads a different structure: duration, speaker ID, text | None. The formats differ. |
| NLS (localized strings) | `NLSParser.h`, key followed by text | Same concept. Nothing new to label. |
| NAM (materials) | Not used. Ghost uses NSA/NAS shaders (`NODMaterialLibrary.cpp:118,152`) | Background only. |
| NAG, NCD, NDD, NDG, NMF, NQD, NSC, NSL, NSS, NST, NTT, NVO | No reference in `src/` | None. Most are VtMR game-specific (clans, disciplines, chronicles). |

### 4.2 NOT property overlap

**Measured** with `comm` and `join` on the uppercase names. The Ghost ID is
the array index (`NODProperties.cpp:89`). The VtMR ID is the `[N]` section
header in `[SDK-TAGS]`.

| Set | Count |
|---|---|
| Ghost engine table | 177 |
| VtMR tag file | 224 |
| Shared names | 134 |
| Ghost only | 43 |
| VtMR only | 90 |
| Shared names with the **same numeric ID** | 3 |
| Shared names with a **compatible data type** | 132 of 134 |

Key points:

- **IDs differ.** Example: `DESCRIPTIONID` is ID 82 in `[SDK-TAGS]` and ID 47
  in the Ghost table. Never map by ID.
- **Types agree.** 132 of 134 shared names have compatible types. This
  supports matching by name.
- Type mapping used:
  - STRING, FILENAME, FILENAMENOEXT, TEMPLATE → `string32` (0)
  - FLOAT, UFLOAT → `float32` (2)
  - UINT, SINT, FLAGS, ENUM → `uint32` (3)
  - VECTOR, RGB → `vector3` (4)
- The 2 mismatches:
  - `ITEM_AMOUNT`: VtMR UINT, Ghost float32.
  - `PARTICLENAME`: VtMR STRING, Ghost native type 68.
  - Both need a "type differs in Ghost" note.

Ghost-only names (no VtMR label available):
`ALARM_GROUP`, `BASEDON`, `COLLIDE`, `CONTAINER_FLAGS`,
`CONTAINER_HAVOKBREAK_MODEL`, `CONTAINER_HAVOKBREAK_TEMPLATE`,
`CONTAINERHEALTH`, `DESTROYTEMPLATE`, `EDITOR_SUBCATEGORY`, `EFFECT1`-`EFFECT4`,
`EMITTER1`-`EMITTER4`, `HALO_COLOR`, `HALO_MATERIAL`, `HALO_SIZE`,
`ITEM_FIREPOS`, `MODEL_LODSCALE`, `MODELOFFSET`, `ORIENTATION`, `PHYSMODEL`,
`POSITION`, `PROP_FLAGS`, `RENDERFLAGS`, `RETICULE_SCALE`, `SCORE_REWARD`,
`SECTOR`, `SENSOR_FLAGS`, `SKELETON_ID`, `SOUND_TYPE`, `STAT_ENERGY`,
`STAT_ENERGYPOOL`, `TEMPLATE_MOVETYPE`, `VEHICLEDESC`, `WEAPON_BLADETRAIL`,
`WEAPON_MAXDIST2`, `WEAPON_MINDIST2`, `WEAPON_TYPE1`, `WEAPON_TYPE2`.

**Suggested** renames. The names are similar, not equal. Do not apply them without Ghost data:

| Ghost name | Possible VtMR name | VtMR table |
|---|---|---|
| `basedon` | `PARENT` | none |
| `COLLIDE` | `COLLIDETYPE` | `ENUM_COLLIDETYPE` |
| `TEMPLATE_MOVETYPE` | `MOVETYPE` | `ENUM_MOVETYPE` |
| `SOUND_TYPE` | `SOUNDTYPE` | `ENUM_SOUNDTYPE` |
| `RENDERFLAGS` | `THINGRENDERFLAGS` | `FLAG_THINGRENDERFLAGS` |
| `WEAPON_TYPE1`, `WEAPON_TYPE2` | `WEAPON_TYPE` | `ENUM_WEAPON_TYPE` |

Category codes in `[SDK-TAGS]`: 0 General, 10 Actor, 20 Item, 30 Prop,
40 Projectile, 50 Region, 60 Physics, 99 Embrace-specific. The header also
lists 13 Player, 23 Weapon and 26 Armor as "not implemented yet". Code 70 is used by
`EXPLOSION_FLAGS` and `EXPLOSION_NAME` but is not listed in the file header.

## 5. Proposal 1: NAD tag labels

### Data

Tag values from `[SDK-NAD]` ("Keyframe Tag Meanings"). The values are **Fact**
for VtMR. For Ghost they are **Suggested** until the survey below confirms
them. Descriptions are our own summaries.

| Value | VtMR name | Meaning (paraphrased) |
|---|---|---|
| 0 | Lwalk | Left footstep, walk |
| 1 | Rwalk | Right footstep, walk |
| 2 | Lrun | Left footstep, run |
| 3 | Rrun | Right footstep, run |
| 4 | Fire | Spawn projectile |
| 5 | Strike | Melee hit check |
| 6 | Cast | Trigger cast |
| 7 | Fall | Body-hits-ground sound |
| 8 | Project | Show projectile mesh in hand |
| 9 | Flap | Flying sound |
| 10 | Suck | Feeding sound |
| 11 | Idle | Idle sound |
| 12 | Idle2 | Second idle sound |
| 13-15 | Codex1-Codex3 | Script callback |
| 16 | Repeat | Check held attack, jump to RepeatTo |
| 17 | RepeatTo | Jump target for Repeat |
| 18 | Throwdeath | Check victim death during throw |

### Changes

All items here are **Suggested**.

- New header `src/parsers/NODLabels.h` with
  `const char *nadTagName(uint32_t value)`. It returns `nullptr` when the value is unknown.
- `src/viewer/ui/Inspectors.cpp:333`: show `Frame 12.0: tag 4 (Fire?)`.
  The `?` marks an unverified VtMR name.
- `src/tools/nodexport/nodexport_nad.cpp:73`: add `"name"` and
  `"name_source":"vtmr_sdk_2000"` next to `"type"`. Keep `"type"` unchanged.

### Verification before marking a name as verified

- `NODSurvey nad` only counts tags today (`src/tools/nodsurvey/nodsurvey.cpp:145`).
  Extend it with a histogram of tag values.
- Check that footstep tags (0-3) appear in walk and run clips.
- Check that Fire (4) appears in weapon clips.
- Values above 18 stay unnamed.

## 6. Proposal 2: Property tooltips

### Data

- One table entry per shared name (134 entries, Appendix A).
- Fields: name, VtMR data type, VtMR category, a short description in our own words.
- Matching is case-insensitive and **by name only**.

### Changes

All items here are **Suggested**.

- Add `const char *nodPropertyHint(const char *name)` to `NODLabels.h`.
- `src/tools/editors/editor_app.cpp:854` (the add-property panel): show the
  hint under the existing "Tag %u; native type %u; engine handler %u." line.
- `src/tools/editors/document.cpp:112` (the field label for an existing
  property): show the same hint as a tooltip.
- Add a type warning for `ITEM_AMOUNT` and `PARTICLENAME`.
- Ghost-only names show no hint.

### Rules

- Never change `nodPropertyInfo()` or the native types.
- Hints are display text only. They must not affect validation or compiling.

## 7. Proposal 3: NMS validation in NODSurvey

### Data from `[SDK-NMS]`

Motion mode families:

- Movement: `walk`, `run`, `stand`, `idle`, `limp`, plus weapon suffixes
  (`_onehand`, `_axe2`, `_sword2`, `_polearm`, `_gun`, `_gun2`, `_bow`,
  `_shotgun`, `_crossbow`, `_rocket`, `_chainsaw`, `_chaingun`, `_util`,
  `_util_onehand`, `_util_gun`).
- Combat: for example `claw`, `swordslash`, `pistol`, `rifleaimed`,
  `machinegun`, `toss`, `uzi`, `flamethrower`, `shield`, `hurt`, `util_*`.
- Misc: for example `damagelight`, `damagemed`, `damageheavy`, `talk`,
  `listen`, `create`, `death*`, `special1`-`special25`, `gesture1`-`gesture10`,
  `action1`-`action10`, `crouch`, `hidden`.
- The VtMR feeding and discipline modes are game-specific. **Suggested:**
  Ghost is unlikely to use them.

Flags (complete list): `noloop`, `movespeed`, `highpriority`,
`lowpriority`, `endpause`, `endtag`, `variable`, `special`, `nofadeout`,
`nofadein`, `fidget`.

Other line types: `tag <joint> <bone> [offset]`, `mouth <a-i> <bone> (rot) [(trans)]`.

**Ambiguous** spellings in `[SDK-NMS]`. They are probably typos in the docs:

- The combat mode is written `Crosbow`. The walk suffix is `_crossbow`.
- The limp weapon family is written `Liml_XXX`. Elsewhere it is `Limp`.

The known list should accept both spellings until Ghost data decides.

### Changes

`NODSurvey nms` already exists (`src/tools/nodsurvey/nodsurvey_nms.cpp`).
It already counts flags and resolves clip files. It does not count motion
modes, and it does not check names against a known list.

- Extend `nodsurvey_nms.cpp`. Do not add a new mode.
- Add a motion mode histogram next to the existing `"flags"` histogram.
- Report entries as:
  - `known-vtmr`: mode and flags all listed in `[SDK-NMS]`.
  - `unknown`: not listed. **Not an error.** Ghost may add modes.
- Report a histogram of unknown modes and flags. This finds Ghost-specific names.
- Never fail a file on an unknown name.

### Note

There are no `.nms` files in the repo. The check needs a Ghost asset tree to run.

## 7a. Proposal 4 (optional): Enum and flag value labels

Confidence: **Suggested**, the lowest in this document. Enum values and flag
bits are the most likely things to change between games.

### Data from `[SDK-DATA]`

- `[SDK-DATA]` has 37 sections. They are `ENUM_*` value lists, `FLAG_*` bit
  lists, file extensions and paths.
- In `[SDK-TAGS]`, a tag points to its list with a `REF=` line.
- **Measured:** 22 of the 134 shared names have a `REF=`:

| Shared name(s) | VtMR list |
|---|---|
| `TYPE` | `ENUM_TYPE` |
| `ACTOR_TYPE` | `ENUM_ACTOR_TYPE` |
| `ITEM_TYPE` | `ENUM_ITEM_TYPE` |
| `DYNA_STYLE` | `ENUM_DYNA_STYLE` |
| `EDITORCATEGORY` | `ENUM_EDITORCATEGORY` |
| `PROJECT_DAMAGETYPE` | `ENUM_PROJECT_DAMAGETYPE` |
| `WEAPON_DAMAGETYPE1`, `WEAPON_DAMAGETYPE2` | `ENUM_WEAPON_DAMAGETYPE` |
| `WEAPON_DEFENSETYPE` | `ENUM_WEAPON_DEFENSETYPE` |
| `WEAPON_MELEETYPE2` | `ENUM_WEAPON_MELEETYPE` |
| `THINGFLAGS` | `FLAG_THINGFLAGS` |
| `MODELFLAGS` | `FLAG_MODELFLAGS` |
| `ACTOR_FLAGS` | `FLAG_ACTOR_FLAGS` |
| `ITEM_FLAGS` | `FLAG_ITEM_FLAGS` |
| `DYNA_FLAGS` | `FLAG_DYNAFLAGS` |
| `WEAPON_FLAGS`, `WEAPON_FLAGS1`, `WEAPON_FLAGS2` | `FLAG_WEAPON_FLAGS` |
| `PROJECT_FLAGS` | `FLAG_PROJECT_FLAGS` |
| `EXPLOSION_FLAGS` | `FLAG_EXPLOSION_FLAGS` |
| `PHYSICS_FLAGS` | `FLAG_PHYSICS_FLAGS` |
| `REGION_FLAGS` | `FLAG_REGION_FLAGS` |

Example (Fact, VtMR): `ENUM_TYPE` is 0 ACTOR, 1 ITEM, 2 PROP, 3 REGION,
4 PROJECTILE, 5 WEAPON, 6 ARMOR, 7 PLAYER, 8 CONTAINER, 9 NONE, 10 EXPLOSION.

One consistent hint in this repo: `src/tools/common/nod_script.cpp:870`
gives region defaults when `owner_type == 3`. That matches VtMR 3 = REGION.
One match does not prove the rest of the table.

### Changes (Suggested)

- Show the VtMR name next to the raw value, for example `3 (REGION?)`.
- Flags: show the VtMR bit names next to the raw hex value.
- Today the editor says "raw bits, not a validated enum" (`editor_app.cpp:857`).
  Keep that text. The labels are hints only.
- Before marking any value as verified, collect value histograms from Ghost NOT files.

## 8. Out of scope

- Porting VtMR binary layouts (NOD v7, NAD v3, NIL v27).
- The VtMR `.npd` particle format. It is unrelated to Ghost NPD path data.
- Using Embrace, NOTEditor, NODView or NODExport (Win32 binaries, VtMR only).
- Codex Java scripts. Ghost uses a different script system (NODScript).

Possible future work (**Suggested**):

- Use `[SDK-ENT]` as a structure reference for
  `src/tools/radiant/entities.def`.
- Read the NMS joint `tag` names (Root, LHip, Head, Weapon, ...) and the
  `mouth` lines for Ghost skeleton labelling.

## 9. Risks

| Risk | Mitigation |
|---|---|
| A VtMR name is wrong for Ghost | Mark it unverified (`?`). Confirm it with corpus data first. |
| Mapping by ID by mistake | Match by name only. Add a unit test for `DESCRIPTIONID` (VtMR 82, Ghost 47). |
| License | Store facts only. Paraphrase all text. Do not copy SDK files. |
| Labels change tool output | Add JSON fields only. Never change existing fields. |
| Enum values or flag bits differ in Ghost | Show the raw value first. Mark the VtMR name `?`. Use value histograms. |
| NAD coefficients mislabeled | Keep the current `b`/`c`/`d` names. Do not import the VtMR C/B/A names. |

## Appendix A: Shared NOT properties

Source columns: `[SDK-TAGS]`. Ghost column: `src/parsers/NODProperties.cpp`.

`ITEM_EFFECTNAME0-9`, `ITEM_EFFECTLEVEL0-9`, `ITEM_EFFECTVALUE0-9` and
`ITEM_EFFECTTIME0-9` (40 entries) all match. Their types are STRING, SINT,
FLOAT and UINT, mapped to string32, uint32, float32 and uint32.

| Name | VtMR type | VtMR cat | Ghost type |
|---|---|---|---|
| `ACTOR_AI` | FILENAME | 10 | string32 |
| `ACTOR_FIREPOS` | VECTOR | 10 | vector3 |
| `ACTOR_FLAGS` | FLAGS | 10 | uint32 |
| `ACTOR_GIBCLASS` | UINT | 10 | uint32 |
| `ACTOR_HEALTH` | UINT | 10 | uint32 |
| `ACTOR_MAXHEALTH` | UINT | 10 | uint32 |
| `ACTOR_RUNSPEED` | UFLOAT | 10 | float32 |
| `ACTOR_TYPE` | ENUM | 10 | uint32 |
| `ACTOR_WALKSPEED` | UFLOAT | 10 | float32 |
| `ACTOR_WEAPON` | TEMPLATE | 10 | string32 |
| `CASTID` | STRING | 10 | string32 |
| `DESCRIPTIONID` | STRING | 0 | string32 |
| `DROPITEM` | TEMPLATE | 10 | string32 |
| `DYNA_BONE` | SINT | 0 | uint32 |
| `DYNA_COLOR` | RGB | 0 | vector3 |
| `DYNA_FLAGS` | FLAGS | 0 | uint32 |
| `DYNA_INTENSITY` | UFLOAT | 0 | float32 |
| `DYNA_MATERIAL` | FILENAMENOEXT | 0 | string32 |
| `DYNA_OFFSET` | VECTOR | 0 | vector3 |
| `DYNA_RADIUS` | UFLOAT | 0 | float32 |
| `DYNA_STYLE` | ENUM | 0 | uint32 |
| `EDITORCATEGORY` | ENUM | 99 | uint32 |
| `EDITORCOLOR` | RGB | 99 | vector3 |
| `EDITORSIZEMAXS` | VECTOR | 99 | vector3 |
| `EDITORSIZEMINS` | VECTOR | 99 | vector3 |
| `EXPLOSION` | TEMPLATE | 0 | string32 |
| `EXPLOSION_FLAGS` | FLAGS | 70 | uint32 |
| `EXPLOSION_NAME` | FILENAME | 70 | string32 |
| `FOLEY` | FILENAME | 0 | string32 |
| `HEIGHT` | UFLOAT | 0 | float32 |
| `ITEM_AMOUNT` | UINT | 20 | float32 (**differs**) |
| `ITEM_FLAGS` | FLAGS | 20 | uint32 |
| `ITEM_ID` | UINT | 20 | uint32 |
| `ITEM_TYPE` | ENUM | 20 | uint32 |
| `MODEL` | FILENAME | 0 | string32 |
| `MODELFLAGS` | FLAGS | 0 | uint32 |
| `MODELSCALE` | UFLOAT | 0 | float32 |
| `MOTIONSET` | FILENAME | 0 | string32 |
| `MOVESPEED` | UFLOAT | 0 | float32 |
| `PARTICLENAME` | STRING | 0 | 68 (**differs**) |
| `PHYSICS_ELASTICITY` | UFLOAT | 60 | float32 |
| `PHYSICS_FLAGS` | FLAGS | 60 | uint32 |
| `PHYSICS_GRAVITY` | FLOAT | 60 | float32 |
| `PHYSICS_MASS` | UFLOAT | 60 | float32 |
| `PHYSICS_ROTDRAG` | UFLOAT | 60 | float32 |
| `PHYSICS_ROTVEL` | VECTOR | 60 | vector3 |
| `PHYSICS_THRUST` | VECTOR | 60 | vector3 |
| `PHYSICS_VELDRAG` | UFLOAT | 60 | float32 |
| `PHYSICS_VELOCITY` | VECTOR | 60 | vector3 |
| `PROJECT_DAMAGE` | UINT | 40 | uint32 |
| `PROJECT_DAMAGETYPE` | ENUM | 40 | uint32 |
| `PROJECT_FLAGS` | FLAGS | 40 | uint32 |
| `PROJECT_MAXDIST` | UFLOAT | 40 | float32 |
| `PROXRADIUS` | UFLOAT | 0 | float32 |
| `RADIUS` | UFLOAT | 0 | float32 |
| `REGION_FLAGS` | FLAGS | 50 | uint32 |
| `SCRIPT` | FILENAMENOEXT | 0 | string32 |
| `THINGFLAGS` | FLAGS | 0 | uint32 |
| `TIMER` | UINT | 0 | uint32 |
| `TYPE` | ENUM | 0 | uint32 |
| `WALLMARK` | STRING | 0 | string32 |
| `WEAPON_ACCURACY1` | UFLOAT | 20 | float32 |
| `WEAPON_ACCURACY2` | UFLOAT | 20 | float32 |
| `WEAPON_AMMOTYPE1` | UINT | 20 | uint32 |
| `WEAPON_AMMOTYPE2` | UINT | 20 | uint32 |
| `WEAPON_AMMOUSE1` | UINT | 20 | uint32 |
| `WEAPON_AMMOUSE2` | UINT | 20 | uint32 |
| `WEAPON_BLADEEND` | VECTOR | 20 | vector3 |
| `WEAPON_BLADESTART` | VECTOR | 20 | vector3 |
| `WEAPON_DAMAGE1` | UINT | 20 | uint32 |
| `WEAPON_DAMAGE2` | UINT | 20 | uint32 |
| `WEAPON_DAMAGETYPE1` | ENUM | 20 | uint32 |
| `WEAPON_DAMAGETYPE2` | ENUM | 20 | uint32 |
| `WEAPON_DEFENSETYPE` | ENUM | 20 | uint32 |
| `WEAPON_FLAGS` | FLAGS | 20 | uint32 |
| `WEAPON_FLAGS1` | FLAGS | 20 | uint32 |
| `WEAPON_FLAGS2` | FLAGS | 20 | uint32 |
| `WEAPON_MAXAMMO1` | UINT | 20 | uint32 |
| `WEAPON_MAXAMMO2` | UINT | 20 | uint32 |
| `WEAPON_MAXDISTANCE` | UFLOAT | 20 | float32 |
| `WEAPON_MELEETYPE2` | ENUM | 20 | uint32 |
| `WEAPON_MINDISTANCE` | UFLOAT | 20 | float32 |
| `WEAPON_MODEL` | STRING | 20 | string32 |
| `WEAPON_MUZZLEFLASH1` | TEMPLATE | 20 | string32 |
| `WEAPON_MUZZLEFLASH2` | TEMPLATE | 20 | string32 |
| `WEAPON_PROJECTILE` | TEMPLATE | 20 | string32 |
| `WEAPON_PROJECTILE2` | TEMPLATE | 20 | string32 |
| `WEAPON_RELOADTIME` | UFLOAT | 20 | float32 |
| `WEAPON_RELOADTIME2` | UFLOAT | 20 | float32 |
| `WEAPON_STRIKETIME1` | UFLOAT | 20 | float32 |
| `WEAPON_STRIKETIME2` | UFLOAT | 20 | float32 |
| `WEAPON_THROWCHANCE` | UFLOAT | 20 | float32 |
| `WEAPON_THROWDIST` | UFLOAT | 20 | float32 |
| `WEAPON_THROWTYPE` | UINT | 20 | uint32 |

## Appendix B: Reproducing the comparison

Tools: `7z`, `unshield` (built from source), and coreutils
(`grep`, `sed`, `sort`, `nl`, `paste`, `comm`, `join`).

1. `7z x NODSDK.exe` gives `Disk1/data1.cab`.
2. `unshield -d out x Disk1/data1.cab` gives the files listed in section 2.
3. Ghost names: the `{"NAME", handler, type}` entries in
   `src/parsers/NODProperties.cpp`. The ID is the array index.
4. VtMR names: the `[ID]` and `NAME=` / `DATATYPE=` / `CAT=` lines in `[SDK-TAGS]`.
5. Uppercase both name lists. Then use `comm` for set overlap and `join` for the type check.
6. Enum and flag links: the `NAME=` and `REF=` lines in `[SDK-TAGS]`, joined
   with the shared names. The list sections are the `[...]` headers in `[SDK-DATA]`.
