#!/usr/bin/env bash
set -euo pipefail

if [[ $# -eq 1 && $1 == --help ]]; then
    printf 'Usage: %s\nRemove .build (all generated CMake files) and build-out (application executables).\nAlso handles the older CMake layout in build-out. Preserve .deps/vcpkg.\nOther directories are untouched. CMake is not required.\n' "$0"
    exit 0
fi
if [[ $# -ne 0 ]]; then
    printf 'Usage: %s\n' "$0" >&2
    exit 2
fi

repo_root="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
clean_dirs=()
shopt -s nullglob dotglob
for build_dir in "$repo_root/.build" "$repo_root/build-out"; do
    cache="$build_dir/CMakeCache.txt"
    if [[ -L "$build_dir" ]]; then
        printf 'clean.sh: refusing to clean a symlinked directory: %s\n' "$build_dir" >&2
        exit 1
    fi
    if [[ ! -e "$build_dir" ]]; then
        continue
    fi
    if [[ ! -d "$build_dir" ]]; then
        printf 'clean.sh: refusing to clean an unrecognized directory: %s\n' "$build_dir" >&2
        exit 1
    fi
    if [[ -f "$cache" ]]; then
        cache_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")"
        cache_source="${cache_source%$'\r'}"
        if command -v cygpath >/dev/null 2>&1; then
            cache_source="$(cygpath -u "$cache_source")"
        fi
        if [[ "$cache_source" != "$repo_root" ]]; then
            printf 'clean.sh: refusing to clean a build belonging to another source directory.\n' >&2
            exit 1
        fi
    elif [[ ! -d "$build_dir/CMakeFiles" || -L "$build_dir/CMakeFiles" ]]; then
        entries=("$build_dir"/*)
        for entry in "${entries[@]}"; do
            name="${entry##*/}"
            name="${name%.exe}"
            case "$name" in
                NILtoMAP|NODscene|NODtoNMB|MAPtoNIL|MAPtoNSD|MAPtoNPD|NODTemplates|NSDEditor|NOTEditor|NODAssetViewer)
                    if [[ "$build_dir" == "$repo_root/build-out" && -f "$entry" && ! -L "$entry" ]]; then
                        continue
                    fi
                    ;;
            esac
            printf 'clean.sh: refusing to remove an unrecognized directory: %s\n' "$build_dir" >&2
            exit 1
        done
    fi
    clean_dirs+=("$build_dir")
done

for build_dir in "${clean_dirs[@]}"; do
    rm -rf -- "$build_dir"
    printf 'clean.sh: removed %s\n' "$build_dir"
done
if [[ ${#clean_dirs[@]} -eq 0 ]]; then
    printf 'clean.sh: build and output directories are already absent.\n'
fi