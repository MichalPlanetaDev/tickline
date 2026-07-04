#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
    pwd
)"

repository_root="$(
    cd -- "$script_dir/../.."
    pwd
)"

module_directory="$repository_root/tools/tickline-dev"
temporary_directory="$(mktemp -d)"

cleanup() {
    rm -rf -- "$temporary_directory"
}

trap cleanup EXIT

go -C "$module_directory" test ./...
go -C "$module_directory" test -race ./...
go -C "$module_directory" vet ./...

binary_path="$temporary_directory/tickline-dev"

go -C "$module_directory" build \
    -trimpath \
    -o "$binary_path" \
    ./cmd/tickline-dev

expected_version="tickline-dev 0.5.0"
actual_version="$("$binary_path" version)"

if [[ "$actual_version" != "$expected_version" ]]; then
    printf \
        'unexpected developer-console version: expected %q, got %q\n' \
        "$expected_version" \
        "$actual_version" \
        >&2

    exit 1
fi

"$binary_path" check --plan >/dev/null
