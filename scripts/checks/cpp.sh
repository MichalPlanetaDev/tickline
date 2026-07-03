#!/usr/bin/env bash
set -Eeuo pipefail

repository_root="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &&
    pwd
)"

cd "$repository_root"

cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel

ctest \
    --test-dir build \
    --output-on-failure
