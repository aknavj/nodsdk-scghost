#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf 'Usage: %s [NILtoMAP options] input.nil output.map\n' "$0"
    printf '       %s --asset-root DIR [--scale NUMBER] [--thickness NUMBER] [--classic]\n' "$0"
}

if [[ $# -eq 1 && $1 == --help ]]; then
    usage
    exit 0
fi
if [[ $# -eq 0 ]]; then
    usage >&2
    exit 2
fi

repo_root="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
app_dir="$repo_root/build-out"
build_dir="$repo_root/build/niltomap"
converter=""

for executable in "$app_dir/NILtoMAP" "$app_dir/NILtoMAP.exe" \
                  "$app_dir/Release/NILtoMAP" "$app_dir/Release/NILtoMAP.exe" \
                  "$build_dir/NILtoMAP" "$build_dir/NILtoMAP.exe" \
                  "$build_dir/Release/NILtoMAP" "$build_dir/Release/NILtoMAP.exe"; do
    if [[ -f "$executable" && -x "$executable" ]]; then
        converter="$executable"
        break
    fi
done

if [[ -z "$converter" ]]; then
    printf 'nil2map.sh: NILtoMAP was not found in %s or %s; run build.sh first.\n' "$app_dir" "$build_dir" >&2
    exit 127
fi

asset_root=""
arguments=()
options=true
while [[ $# -gt 0 ]]; do
    if [[ "$options" == true && $1 == --asset-root ]]; then
        if [[ $# -lt 2 || -z "$2" || -n "$asset_root" ]]; then
            printf 'nil2map.sh: specify --asset-root once with a directory.\n' >&2
            exit 2
        fi
        asset_root="$2"
        shift 2
    else
        if [[ $1 == -- ]]; then options=false; fi
        arguments+=("$1")
        shift
    fi
done
if [[ -z "$asset_root" ]]; then
    exec "$converter" "${arguments[@]}"
fi

batch_options=()
index=0
while [[ $index -lt ${#arguments[@]} ]]; do
    argument="${arguments[$index]}"
    case "$argument" in
        --scale|--thickness)
            if [[ $((index + 1)) -ge ${#arguments[@]} ]]; then
                printf 'nil2map.sh: %s requires a number.\n' "$argument" >&2
                exit 2
            fi
            batch_options+=("$argument" "${arguments[$((index + 1))]}")
            index=$((index + 2))
            ;;
        --classic)
            batch_options+=("$argument")
            index=$((index + 1))
            ;;
        *)
            printf 'nil2map.sh: unexpected batch argument: %s\n' "$argument" >&2
            exit 2
            ;;
    esac
done

if command -v cygpath >/dev/null 2>&1; then
    asset_root="$(cygpath -u "$asset_root")"
fi
if [[ ! -d "$asset_root" ]]; then
    printf 'nil2map.sh: asset directory does not exist: %s\n' "$asset_root" >&2
    exit 1
fi
asset_root="$(CDPATH= cd -- "$asset_root" && pwd -P)"
levels=()
if [[ "${asset_root##*/}" == [Ll][Ee][Vv][Ee][Ll][Ss] ]]; then
    levels=("$asset_root")
else
    shopt -s nullglob
    for directory in "$asset_root"/*; do
        if [[ -d "$directory" && ! -L "$directory" && "${directory##*/}" == [Ll][Ee][Vv][Ee][Ll][Ss] ]]; then
            levels+=("$directory")
        fi
    done
fi
if [[ ${#levels[@]} -ne 1 ]]; then
    printf 'nil2map.sh: expected one Levels directory in %s; found %s.\n' "$asset_root" "${#levels[@]}" >&2
    exit 1
fi
levels_dir="${levels[0]}"
output_dir="$repo_root/maps-out"
if [[ "$output_dir/" == "${asset_root%/}/"* ]]; then
    printf 'nil2map.sh: maps-out must be outside the original asset directory.\n' >&2
    exit 1
fi
if [[ -L "$output_dir" || ( -e "$output_dir" && ! -d "$output_dir" ) ]]; then
    printf 'nil2map.sh: refusing to use a symlinked or non-directory maps-out.\n' >&2
    exit 1
fi
if [[ -d "$output_dir" ]]; then
    symlink="$(find "$output_dir" -type l -print -quit)"
    if [[ -n "$symlink" ]]; then
        printf 'nil2map.sh: refusing to write through a symlink in maps-out: %s\n' "$symlink" >&2
        exit 1
    fi
fi

list_file="$(mktemp)"
staged_output=""
cleanup() {
    if [[ -n "$staged_output" ]]; then rm -f -- "$staged_output"; fi
    rm -f -- "$list_file"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP
find "$levels_dir" -type f -iname '*.nil' -print0 > "$list_file"
sources=()
outputs=()
while IFS= read -r -d '' source; do
    relative="${source#"$levels_dir"/}"
    output="$output_dir/${relative%.*}.map"
    for previous in "${outputs[@]}"; do
        if [[ "$previous" == "$output" ]]; then
            printf 'nil2map.sh: multiple NIL files map to the same output: %s\n' "$output" >&2
            exit 1
        fi
    done
    sources+=("$source")
    outputs+=("$output")
done < "$list_file"
if [[ ${#sources[@]} -eq 0 ]]; then
    printf 'nil2map.sh: no NIL files found in %s.\n' "$levels_dir" >&2
    exit 1
fi

converted=0
failed=0
for ((index = 0; index < ${#sources[@]}; ++index)); do
    source="${sources[$index]}"
    output="${outputs[$index]}"
    parent="${output%/*}"
    if [[ -e "$output" && ! -f "$output" ]]; then
        printf 'nil2map.sh: output is not a regular MAP file: %s\n' "$output" >&2
        failed=$((failed + 1))
        continue
    fi
    if ! mkdir -p -- "$parent"; then
        printf 'nil2map.sh: cannot create output directory: %s\n' "$parent" >&2
        failed=$((failed + 1))
        continue
    fi
    if ! staged_output="$(mktemp "$parent/.nil2map.XXXXXX")"; then
        printf 'nil2map.sh: cannot stage output for %s.\n' "$source" >&2
        failed=$((failed + 1))
        continue
    fi
    if "$converter" "${batch_options[@]}" -- "$source" "$staged_output" && mv -f -- "$staged_output" "$output"; then
        printf 'nil2map.sh: converted %s -> %s\n' "$source" "$output"
        converted=$((converted + 1))
    else
        printf 'nil2map.sh: conversion failed: %s\n' "$source" >&2
        failed=$((failed + 1))
    fi
    rm -f -- "$staged_output"
    staged_output=""
done
printf 'nil2map.sh: %s converted, %s failed; output: %s\n' "$converted" "$failed" "$output_dir"
if [[ $failed -ne 0 ]]; then exit 1; fi