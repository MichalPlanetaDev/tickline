#!/usr/bin/env bash
set -Eeuo pipefail

repository_root="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." &&
    pwd
)"

module_root="$repository_root/tools/tickline-dev"

cd "$module_root"

unformatted="$(
    find . \
        -type f \
        -name '*.go' \
        -exec gofmt -l {} +
)"

if [[ -n "$unformatted" ]]; then
    printf '%s\n' "Go files require formatting:" >&2
    printf '%s\n' "$unformatted" >&2
    exit 1
fi

go test ./...
go vet ./...
