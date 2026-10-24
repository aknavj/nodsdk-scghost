#!/usr/bin/env bash
set -euo pipefail

if [[ $# -eq 1 && $1 == --help ]]; then
    printf 'Usage: %s\nBuild all Release tools, the viewer, and NSD/NOT editors.\nOnly application executables are copied to build-out; CMake files stay in .build.\nRequires CMake and C/C++ compilers. Windows restores static dependencies with vcpkg,\nusing VCPKG_ROOT or .deps/vcpkg (downloaded automatically; requires Git and network access).\nCMAKE_TOOLCHAIN_FILE overrides automatic setup. Linux/macOS require GLM, OpenGL, GLFW, and GLEW.\n' "$0"
    exit 0
fi
if [[ $# -ne 0 ]]; then
    printf 'Usage: %s\n' "$0" >&2
    exit 2
fi

repo_root="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
build_dir="$repo_root/.build"
output_dir="$repo_root/build-out"
apps=(NILtoMAP NODscene NODtoNMB MAPtoNIL MAPtoNSD MAPtoNPD NODTemplates NSDEditor NOTEditor NODAssetViewer)
exe_suffix=""
cmake_args=()

for directory in "$build_dir" "$output_dir"; do
    if [[ -L "$directory" ]]; then
        printf 'build.sh: refusing to use a symlinked build/output directory: %s\n' "$directory" >&2
        exit 1
    fi
done
if [[ -f "$output_dir/CMakeCache.txt" || -d "$output_dir/CMakeFiles" ]]; then
    printf 'build.sh: build-out contains an older CMake build; run bash clean.sh first.\n' >&2
    exit 1
fi
shopt -s nullglob dotglob
for entry in "$output_dir"/*; do
    recognized=false
    for app in "${apps[@]}"; do
        if [[ ( "${entry##*/}" == "$app" || "${entry##*/}" == "$app.exe" ) && -f "$entry" && ! -L "$entry" ]]; then
            recognized=true
            break
        fi
    done
    if [[ "$recognized" == false ]]; then
        printf 'build.sh: build-out contains an unexpected file or directory: %s\n' "$entry" >&2
        exit 1
    fi
done

if ! command -v cmake >/dev/null 2>&1; then
    printf 'build.sh: CMake is required but was not found in PATH.\n' >&2
    exit 127
fi

toolchain="${CMAKE_TOOLCHAIN_FILE:-}"
if [[ -z "$toolchain" && -f "$build_dir/CMakeCache.txt" ]]; then
    toolchain="$(sed -n 's/^CMAKE_TOOLCHAIN_FILE:[^=]*=//p' "$build_dir/CMakeCache.txt")"
    toolchain="${toolchain%$'\r'}"
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        exe_suffix=".exe"
        cmake_args+=("-DVCPKG_TARGET_TRIPLET=x64-windows-static"
            '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>')
        if [[ -z "$toolchain" ]]; then
            vcpkg_root="${VCPKG_ROOT:-"$repo_root/.deps/vcpkg"}"
            vcpkg_root="$(cygpath -u "$vcpkg_root")"
            if [[ ! -e "$vcpkg_root" ]]; then
                if ! command -v git >/dev/null 2>&1; then
                    printf 'build.sh: Git is required to download vcpkg.\n' >&2
                    exit 127
                fi
                mkdir -p -- "$(dirname -- "$vcpkg_root")"
                git clone https://github.com/microsoft/vcpkg.git "$vcpkg_root"
            fi
            toolchain="$vcpkg_root/scripts/buildsystems/vcpkg.cmake"
            if [[ ! -f "$toolchain" || ! -f "$vcpkg_root/bootstrap-vcpkg.bat" ]]; then
                printf 'build.sh: invalid vcpkg installation at %s.\n' "$vcpkg_root" >&2
                exit 1
            fi
            if [[ ! -f "$vcpkg_root/vcpkg.exe" ]]; then
                (
                    cd -- "$vcpkg_root"
                    MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
                        cmd.exe /d /c bootstrap-vcpkg.bat -disableMetrics
                )
            fi
        fi
        if [[ -n "$toolchain" ]]; then
            toolchain="$(cygpath -u "$toolchain")"
        fi
        ;;
esac

if [[ -n "$toolchain" ]]; then
    if [[ ! -f "$toolchain" ]]; then
        printf 'build.sh: CMake toolchain file was not found: %s\n' "$toolchain" >&2
        exit 1
    fi
    cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$toolchain")
fi

cmake -S "$repo_root" -B "$build_dir" "${cmake_args[@]}" -DCMAKE_BUILD_TYPE=Release \
    -DNODSDK_BUILD_PARSERS=ON -DNODSDK_BUILD_VIEWER=ON -DNODSDK_BUILD_EDITORS=ON \
    -DBUILD_TESTING=OFF
cmake --build "$build_dir" --config Release --parallel

executables=()
for app in "${apps[@]}"; do
    executable="$build_dir/cmake/Release/$app$exe_suffix"
    if [[ ! -f "$executable" ]]; then
        executable="$build_dir/cmake/$app$exe_suffix"
    fi
    if [[ ! -f "$executable" || ! -x "$executable" ]]; then
        printf 'build.sh: expected application executable was not built: %s\n' "$app" >&2
        exit 1
    fi
    executables+=("$executable")
done

mkdir -p -- "$output_dir"
cp -- "${executables[@]}" "$output_dir/"
printf 'build.sh: application executables are in %s\n' "$output_dir"