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

destination_directory="${1:-${XDG_BIN_HOME:-$HOME/.local/bin}}"

build_directory="$(mktemp -d)"
temporary_install=""

cleanup() {
    rm -rf -- "$build_directory"

    if [[ -n "$temporary_install" ]]; then
        rm -f -- "$temporary_install"
    fi
}

trap cleanup EXIT

TICKLINE_BUILD_DIR="$build_directory" \
    "$repository_root/scripts/build-tickline-dev.sh"

source_binary="$build_directory/tickline-dev"
destination_binary="$destination_directory/tickline-dev"

mkdir -p -- "$destination_directory"

temporary_install="$(
    mktemp "$destination_directory/.tickline-dev.XXXXXXXX"
)"

install \
    -m 0755 \
    "$source_binary" \
    "$temporary_install"

mv -f -- \
    "$temporary_install" \
    "$destination_binary"

temporary_install=""

printf 'installed %s\n' "$destination_binary"
"$destination_binary" version
