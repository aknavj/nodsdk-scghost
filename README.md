# OpenNOD SDK - SC:Ghost Reversed

Tools and C++17 parsers for Nihilistic Software's NOD engine, based on an early
Xbox prototype of StarCraft: Ghost. An educational reverse-engineering and
preservation project.

![NIL level - Mar Sara](imgs/scg_img0.png)

| | | |
|---|---|---|
| ![Asset browser - NIL level](imgs/scg_img1.png) | ![NAD animation inspector](imgs/scg_img2.png) | ![DDS texture preview](imgs/scg_img3.png) |
| ![NSD scene data](imgs/scg_img4.png) | ![XWB wave bank audio](imgs/scg_img5.png) | |

## Included applications

| Application | Purpose |
|---|---|
| `NODAssetViewer` | Browse models, levels, textures, animations, scenes, and audio assets |
| `NSDEditor`, `NOTEditor` | Inspect and edit native scene and template documents |
| `NODscene` | Import/export scene data, apply sidecar-backed edits, and generate previews or entity definitions |
| `NILtoMAP` | Convert NIL render geometry to editable Quake III MAP brushes used by [nodsdk-radiant](https://github.com/aknavj/nodsdk-radiant) |
| `MAPtoNIL` | Compile MAP world geometry and collision; excludes point-entity placements |
| `MAPtoNSD` | Compile native template placements into NSD scenes |
| `MAPtoNPD` | Compile manual waypoints and directed links into NPD path data |
| `NODtoNMB` | Bundle NOD/NOC models into NMB files |
| `NODTemplates` | Query template/instance metadata and perform validated shared-property edits |

The repository also includes the [parser library](src/parsers/), a
[Blender plugin](plugin/bpy-nodsdk-scghost/), and
[detailed tool documentation](src/tools/README.md).

## Build

Requires Git, CMake 3.16 or later, a C99/C++17 compiler, and Bash.

### Windows

Install **Git for Windows**, **CMake**, and **Visual Studio 2022** with the
**Desktop development with C++** workload and a Windows SDK. Run the scripts
from Git Bash.

The build automatically sets up vcpkg in `.deps/vcpkg` and restores the
[manifest dependencies](vcpkg.json). Initial setup requires network access.
Windows builds use static dependencies and the static MSVC runtime, so no
additional DLLs need to accompany the executables. Each executable includes
the shared icon and Windows version metadata.

To reuse an existing vcpkg installation:

```bash
VCPKG_ROOT="C:/dev/vcpkg" bash build.sh
```

`CMAKE_TOOLCHAIN_FILE` can override the toolchain; custom Windows toolchains
must provide static dependencies. Clean before switching toolchains.

### Linux / macOS

Install OpenGL development files, GLFW 3.3+, GLEW, and GLM.
For Ubuntu / Debian:

```bash
sudo apt install git cmake g++ libgl1-mesa-dev libglfw3-dev libglew-dev libglm-dev
```

### Build all applications

From the repository root:

```bash
git submodule update --init --recursive
bash build.sh
```

[build.sh](build.sh) builds all ten applications in Release:

- `build-out/` contains only the application executables.
- `.build/` contains CMake files, libraries, installed dependencies, and intermediates.
- `.deps/vcpkg/` holds the reusable Windows package-manager checkout.

Run `bash clean.sh` to remove `.build/` and `build-out/` completely.
[clean.sh](clean.sh) preserves `.deps/vcpkg/` and leaves other directories untouched.
Both scripts work from any working directory and accept `--help`.

## Usage

After building, launch the viewer with the root of the extracted game asset
tree (the directory containing folders such as `models/` and `textures/`):

```bash
# Windows / Git Bash
./build-out/NODAssetViewer.exe "C:/path/to/assets"

# Linux / macOS
./build-out/NODAssetViewer /path/to/assets
```

Browse assets by type and select an entry to preview it. Audio playback uses
`aplay` on supported POSIX builds; Windows audio playback is disabled.

Every application provides command-line help:

```bash
./build-out/NODscene.exe --help
./build-out/MAPtoNIL.exe --help
```

On Linux/macOS, omit `.exe`. Tools that require inputs also print help when
launched without arguments. The editors can open their GUI without arguments.

For scene authoring, compiler options, workspace rules, and format limitations,
see the [tools guide](src/tools/README.md). Compiler outputs must remain inside
the authoring workspace and outside the original asset tree.

## Blender plugin

Requires Blender 4.0 or later; no additional Python packages are needed.
Package the plugin:

```bash
cd plugin
zip -rq bpy-nodsdk-scghost.zip bpy-nodsdk-scghost/ -x '*/__pycache__/*'
```

Install the ZIP through Blender's add-on preferences and enable
**OpenNod SDK-scghost**. The **File > Import** menu provides NOD, NMB, NIL,
and NAD importers. NAD animation import requires an existing armature.

## Project scope and license

This is not a game remake or a distribution of proprietary game assets.
It is intended for educational and preservation work, not commercial use.
Respect the rights of the original authors.

Licensed under GNU GPL v3. See individual source files for copyright notices.

This project uses AI-assisted development with collaborative human/AI oversight
and human review of output.
