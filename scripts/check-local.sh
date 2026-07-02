#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-build}"
SANITIZED_BUILD_DIR="${SANITIZED_BUILD_DIR:-build-sanitized}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
SKIP_SANITIZERS="${SKIP_SANITIZERS:-0}"
SKIP_DOCKER="${SKIP_DOCKER:-0}"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "required command not found: $1" >&2
    exit 1
  fi
}

run() {
  echo
  echo "==> $*"
  "$@"
}

require_command git
require_command cmake
require_command ctest
require_command python3

if [[ "$SKIP_DOCKER" != "1" ]]; then
  require_command docker
fi

run git diff --check

run bash scripts/check-docs.sh

run cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
run cmake --build "$BUILD_DIR" --parallel
run ctest --test-dir "$BUILD_DIR" --output-on-failure

if [[ "$SKIP_SANITIZERS" != "1" ]]; then
  run cmake -S . -B "$SANITIZED_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DTICKLINE_ENABLE_SANITIZERS=ON
  run cmake --build "$SANITIZED_BUILD_DIR" --parallel
  run ctest --test-dir "$SANITIZED_BUILD_DIR" --output-on-failure
else
  echo
  echo "==> skipping sanitizer build because SKIP_SANITIZERS=1"
fi

run env PYTHONPATH=tools/python python3 -m unittest discover -s tools/python/tests -v

if [[ "$SKIP_DOCKER" != "1" ]]; then
  run docker build -f infra/docker/Dockerfile .
else
  echo
  echo "==> skipping Docker build because SKIP_DOCKER=1"
fi

echo
echo "local quality gate passed"
