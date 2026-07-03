#!/usr/bin/env bash
set -Eeuo pipefail

repository_root="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &&
    pwd
)"

cd "$repository_root"

cmake \
    -S . \
    -B build-sanitized \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTICKLINE_ENABLE_SANITIZERS=ON

cmake --build build-sanitized --parallel

ctest \
    --test-dir build-sanitized \
    --output-on-failure
