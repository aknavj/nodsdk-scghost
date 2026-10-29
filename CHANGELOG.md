# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.6.8] - T.B.D

### Added

- `MAPtoNIL --physics-margin N` to configure automatic physics-bounds padding in MAP units without changing the one-unit default; explicit `--physics-bounds` remains a mutually exclusive alternative.
- Scriptless single-player startup guidance covering the reported origin spawn, embedded-collision failure, safe authoring workaround and manual reproduction checklist, with runtime-specific validation limits.
- `MAPtoNSD` worldspawn keys `nod_sky_model` and `nod_sky_animation` for validated, fixed-width native sky references; all other trailer fields and default output remain unchanged.
- `MAPtoNIL` pass-through for DDS-backed archived NSA materials, preserving native material names and editor-image UV dimensions while retaining resource, ambiguity and geometry validation.
- `MAPtoNSD` worldspawn keys `nod_music` and `nod_ambient_loop` for validated native audio references, with original-reader verification; numeric environment defaults remain unchanged and playback is not established.
- `MAPtoNSD` linear fog authoring through `nod_fog_mode`, `nod_fog_color`, `nod_fog_start` and `nod_fog_end`, with complete-group validation and original reader/render-state checks; omitted settings retain zero-filled defaults.
- `MAPtoNSD` worldspawn key `nod_far_clip` for the native camera far-clip distance, preserving the zero/default fallback and verified through the original loader and two-camera setter.
- `MAPtoNSD` worldspawn key `nod_reverb` for native preset IDs 0..25, with strict decimal validation and original loader/mixer/backend handoff checks; default output is unchanged and audible reverb is not established.
- Optional native player-placement regression tests and precise startup guidance: routing depends on placement flag `0x8` or save restoration, with origin fallbacks for missing sectors/starts; full controller physics remains unverified.
- Optional `CollisionStartNative` reproduction of first-step test-sphere NaNs on compiled brush surfaces with both 1- and 100-unit physics margins, plus finite clear-start and partial-overlap controls. Read-only contact tracing identifies zero-distance normalization producing NaN normals before the body becomes non-finite (issue #2, item 4); this is not a player/controller fix.
- Optional `MusicGainNative` checks for profile-driven float gain, native clamping/caching and logarithmic XACT volume handoff; distinguish this verified API from the still-unverified uint32 NSD volume fields without adding speculative authoring keys.

### Fixed

- Include the partial subdivision triangle count and active `nod_light_spacing` in lighting triangle-limit errors. The count is not a final mesh estimate; increase spacing to reduce subdivision.
- Name required waypoint/link keys and expected values in `MAPtoNPD` and `MAPtoNSD` validation errors; document a complete programmatic navigation MAP example.
- Document current and legacy programmatic template-placement forms, including archive context and transforms; make `MAPtoNSD` metadata errors identify the expected keys and native values.
- Write collision-group offsets only for nonzero mapping-presence flags, matching the NIL decoder and format reference; cross-sector alignment and every mapping-tail truncation boundary are regression-tested, and current all-present compiler output is unchanged.
- Document the remaining NSD environment words from executable-matched PDB types and original-reader layout probes; keep unverified volume/effects controls explicitly unsupported.

## [0.6.7] - 2026-10-24

### Added

- `NILtoMAP` geometry conversion and `NODscene` scene import/export, source-preserving round-trips, typed property editing, previews, and entity-definition generation.
- `MAPtoNIL` world compilation with brush CSG, texture-aware UVs, generated Havok mesh/MOPP collision, and single-sector NIL output.
- CPU lighting with point lights, sun, ambient illumination, vertex-color baking, shadow sampling, and normal smoothing.
- `MAPtoNSD` native template-placement compilation and `MAPtoNPD` manual waypoint/directed-link graph generation.
- `NODtoNMB` model-bundle compilation from NOD/NOC assets, preserving supported geometry, skeletons, markers, and vertex colors.
- Native NSD/NOT editors with typed edits, local overrides, ordered inheritance, template/archive operations, undo/redo, and protected workspace saves.
- `NODTemplates` XML catalogs, placement metadata, and validated shared-property editing for Radiant workflows.
- Shared NOT archive resolution, native property metadata, model/region bounds extraction, NIL collision decoding, and Radiant entity definitions.
- Batch NIL-to-MAP conversion from asset level directories, preserving subdirectories and existing MAPs when conversion fails.
- Shared Windows application icon and per-executable version metadata.
- Regression coverage for model parsing, disabled audio, standardized CLI help, and batch conversion.

The world, placement, navigation, and bundle compilers remain separate; automatic
asset collection and complete in-game level compatibility are not provided.

### Changed

- Replace synthetic NSD entity classification with ordered NOT template resolution and explicit property provenance.
- Resolve viewer model previews from effective template/placement values, including native model scale and offsets; improve scene and template inspection.
- Share converter, document, CLI, and ImGui backend infrastructure across tools and applications.
- Build all ten applications into an executable-only `build-out/`, with CMake files and intermediates in `.build/`.
- Add Windows dependency restoration through vcpkg with static linking; disable unsupported audio playback while retaining asset inspection.
- Clean generated build and output directories while retaining the reusable vcpkg checkout.
- Standardize CLI help, missing-argument usage, and application window titles.
- Streamline the main README and document scene authoring, compiler limitations, workspace protection, and native lighting.

### Removed

- In-repository format-reference and implementation documents, moved to [nodsdk-docs](https://github.com/aknavj/nodsdk-docs).

### Fixed

- Correct NOT/NSD modifier decoding and inheritance handling, including repeated references, local overrides, and unresolved-template diagnostics.
- Correct NOD/NMB vertex-set, shared-index, and LOD handling; support additional vertex streams and reject malformed or truncated model data.
- Improve parser cleanup on failed/repeated loads and Windows-compatible cutscene marker scanning.
- Restore the missing shared modifier-encoder declaration required to build MAPtoNSD.