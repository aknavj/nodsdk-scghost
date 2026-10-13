# OpenNOD SDK — SC:Ghost Reversed

An ongoing reverse-engineering effort focused on Nihilistic Software's NOD engine, the technology that powered StarCraft: Ghost during its development between 2001–2004. Based on an early Xbox prototype build.

![NIL level — Mar Sara](imgs/scg_img0.png)

| | | |
|---|---|---|
| ![Asset browser — NIL level](imgs/scg_img1.png) | ![NAD animation inspector](imgs/scg_img2.png) | ![DDS texture preview](imgs/scg_img3.png) |
| ![NSD scene data](imgs/scg_img4.png) | ![XWB wave bank audio](imgs/scg_img5.png) | |

## What's inside

| Component | Description |
|---|---|
| `src/parsers/` | C++17 static library — parsers for all major NOD asset formats |
| `src/viewer/` | OpenGL asset browser with ImGui UI |
| `src/tools/` | Standalone pure C NILtoMAP decompiler for GtkRadiant |
| `plugin/bpy-nodsdk-scghost/` | Blender 4.x import plugin (NOD, NMB, NIL, NAD) |

### Parsed formats

| Extension | Content |
|---|---|
| `.nod` | Rigid / skinned mesh model |
| `.nmb` | Multi-body compound mesh bundle |
| `.nil` | Indoor level geometry (sectors + props) |
| `.nad` | Skeletal animation clip |
| `.nak` | Camera keyframe animation |
| `.nsd` | Scene description (entity placement) |
| `.nce` / `.ncs` | Cutscene data |
| `.nco` | Conversation / subtitle data |
| `.nls` / `.nlt` | Localised string tables |
| `.nfx` / `.nlfx` | Particle / effect descriptors |
| `.nui` | UI layout |
| `.not` | Object template definitions |
| `.npd` | Patrol / path data |
| `.noc` | Occlusion / collision mesh |
| `.nms` | Material / surface definition |
| `.nsa` | Sound archive manifest |
| `.nnb` | NavMesh baked data |
| `.xwb` / `.xsb` | Xbox wave bank / sound bank (ADPCM) |
| `.xpr` | Xbox packed resource (textures) |
| `.dds` | DirectDraw Surface texture |
| `.tga` | Targa texture |

---

## Dependencies

### Viewer (Linux / macOS)

| Package | Purpose |
|---|---|
| CMake ≥ 3.16 | build system |
| GCC / Clang (C++17) | compiler |
| OpenGL | rendering |
| GLFW 3.3+ | window + input |
| GLEW | GL extension loader |
| GLM | math library |

Install on Ubuntu / Debian:

```bash
sudo apt install cmake g++ libglfw3-dev libglew-dev libglm-dev
```

Install on Arch:

```bash
sudo pacman -S cmake glfw glew glm
```

### Blender plugin

Blender 4.0 or later. No additional Python packages required.

---

## Build

Initialize the pinned Dear ImGui dependency first:

```bash
git submodule update --init --recursive
```

```bash
cmake -B build/cmake
cmake --build build/cmake --target nodsdk-viewer -j$(nproc)
```

The binary and libraries are written to `cmake/`.

### Windows viewer

Use Visual Studio 2022 with the C++ desktop workload, CMake, and a recent
standalone vcpkg checkout. Older Visual Studio-bundled vcpkg versions can have
obsolete MSYS download URLs.
The dependency manifest pins the registry baseline for GLFW, GLEW, GLM,
and the Windows dirent compatibility header. In PowerShell, with
`VCPKG_ROOT` pointing to the vcpkg installation:

```powershell
cmake -S . -B build\viewer -G "Visual Studio 17 2022" -A x64 `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DNODSDK_ENABLE_AUDIO=OFF
cmake --build build\viewer --config Release --target nodsdk-viewer --parallel
& .\build\viewer\cmake\Release\nodsdk-viewer.exe "C:\path\to\StarCraft Ghost"
```

Windows defaults to audio playback disabled: wave-bank inspection still works,
but playback controls show `n/a` and the inspector explains why. Linux/macOS
retain the existing POSIX backend by default. Model and texture previews are
not disabled. The viewer reads the asset directory; it does not modify assets.

With `BUILD_TESTING` enabled (the default), check the disabled backend with:

```powershell
cmake --build build\viewer --config Release --target nodsdk-audio-disabled-tests
ctest --test-dir build\viewer -C Release -R "^viewer-audio-disabled$" --output-on-failure
```

### Model parser integration

`NODParser` and `NMBParser` also expose `parseBuffer(data, size)` for hosts with
their own virtual filesystem. The input is copied and remains caller-owned;
parsed buffers belong to the parser until its next parse or destruction.
`getLastError()` describes a failed parse. Truncated models, unsupported vertex
layouts, excessive counts, and incomplete bundles are rejected rather than
reported as successful models.
NMB version-one entry boundaries, version-two optional vertex-colour streams,
and tangent-bearing vertex types 3 through 5 are supported. The colour stream
is exposed by each model entry and shares the parser's raw-buffer lifetime.
Read-only validation on the supplied asset tree parsed all 1401 NOD files and
40 NMB bundles successfully.

`tests/model_parsers.cpp` is a standalone synthetic regression driver. Build
it with `AssetParser.cpp`, `NODParser.cpp`, and `NMBParser.cpp`, using C++17 and
the GLM headers. Run it without arguments for regression tests, or pass a NOD
or NMB path for a read-only parser check. Keep generated objects/executables
in a separate build directory. No proprietary assets are needed for the tests.

---

## Running the viewer

```bash
cd cmake
./nodsdk-viewer <assets-directory>
```

`<assets-directory>` should be the root of the extracted game asset tree (the directory that contains subdirectories like `models/`, `textures/`, etc.).

The browser panel on the left lets you browse by asset type. Click any entry to preview it. Supported previews:

- **NOD / NMB** — 3-D mesh with orbital camera, skinning, optional animation playback
- **NIL** — full level geometry with entity instances
- **DDS / TGA / XPR** — texture preview
- **NSD** — entity list with type classification
- **NAD** — animation clip details
- **NCE / NCS** — cutscene actor + event list
- **XWB / XSB** — wave bank audio playback (via `aplay`)

---

## NILtoMAP CLI

Convert NIL v35 render geometry into editable Quake III MAP triangle brushes:

```bash
cmake -S src/tools -B build/niltomap
cmake --build build/niltomap --config Release
NILtoMAP --thickness 1 input.nil output.map
```

Use the built executable's path (`build/niltomap/NILtoMAP`, or
`build/niltomap/Release/NILtoMAP.exe` on Visual Studio). This dependency-free C99
tool reconstructs approximate geometry, not original brush topology, UVs,
lighting, or entities. See [NILtoMAP documentation](src/tools/README.md) for
options, supported binary layout, tests, and limitations.

---

## Blender plugin

### Install

```bash
cd plugin
zip -rq bpy-nodsdk-scghost.zip bpy-nodsdk-scghost/ -x '*/__pycache__/*'
```

In Blender: *Edit → Preferences → Add-ons → Install* → select the zip → enable **OpenNod SDK-scghost**.

### Importers

| Menu entry | Format | Notes |
|---|---|---|
| File → Import → NOD Model (.nod) | `.nod` | rigid or skinned mesh; creates armature if bones present |
| File → Import → NMB Bundle (.nmb) | `.nmb` | multi-body compound mesh |
| File → Import → NIL Level (.nil) | `.nil` | full indoor level; props baked to world space |
| File → Import → NAD Animation (.nad) | `.nad` | animation clip; requires an active armature in the scene |

---

## Project goals

- Open, educational reference on early-2000s console engine design
- Document the NOD engine asset formats completely
- Respect for original authors (Nihilistic Software, Blizzard Entertainment, Activision)

## Why this matters

StarCraft: Ghost is one of the most iconic cancelled games in Blizzard history. The NOD engine also powered *Vampire: The Masquerade — Redemption*. Studying this prototype gives rare insight into Blizzard's early console strategy and Nihilistic's proprietary technology.

---

*Licensed under GNU GPL v3. See individual source files for full copyright notices.*