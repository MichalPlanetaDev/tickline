#!/usr/bin/env bash
set -Eeuo pipefail

repository_root="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &&
    pwd
)"

cd "$repository_root"

export PYTHONPATH="$repository_root/tools/python${PYTHONPATH:+:$PYTHONPATH}"

python3 -m unittest discover \
    -s tools/python/tests \
    -p 'test_*.py' \
    -v
