#!/usr/bin/env bash
set -Eeuo pipefail

repository_root="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &&
    pwd
)"

cd "$repository_root"

docker build \
    --file infra/docker/Dockerfile \
    --tag tickline:local-check \
    .
