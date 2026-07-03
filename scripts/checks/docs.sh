#!/usr/bin/env bash
set -Eeuo pipefail

repository_root="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &&
    pwd
)"

exec bash "$repository_root/scripts/check-docs.sh"
