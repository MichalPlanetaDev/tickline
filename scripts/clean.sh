#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

rm -rf build build-sanitized
rm -rf .pytest_cache .mypy_cache .ruff_cache
find tools cpp -type d -name '__pycache__' -prune -exec rm -rf {} +

echo "local build and test artifacts removed"
