# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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