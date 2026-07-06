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

python_path="$repository_root/tools/python"

if [[ -n "${PYTHONPATH:-}" ]]; then
    python_path="$python_path:$PYTHONPATH"
fi

exec env \
    PYTHONPATH="$python_path" \
    "${PYTHON:-python3}" \
    -m tickline_tools \
    "$@"
