#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
    pwd
)"

repository_root="$(
    cd -- "$script_dir/.."
    pwd
)"

module_directory="$repository_root/tools/tickline-dev"
output_directory="${TICKLINE_BUILD_DIR:-$repository_root/build/tools/tickline-dev}"
output_path="$output_directory/tickline-dev"

mkdir -p -- "$output_directory"

go -C "$module_directory" build \
    -trimpath \
    -o "$output_path" \
    ./cmd/tickline-dev

expected_version="tickline-dev 0.3.0"
actual_version="$("$output_path" version)"

if [[ "$actual_version" != "$expected_version" ]]; then
    printf \
        'built binary reported an unexpected version: expected %q, got %q\n' \
        "$expected_version" \
        "$actual_version" \
        >&2

    rm -f -- "$output_path"
    exit 1
fi

printf 'built %s\n' "$output_path"
printf '%s\n' "$actual_version"
