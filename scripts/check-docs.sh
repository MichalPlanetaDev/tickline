#!/usr/bin/env bash
set -euo pipefail

required_files=(
  "README.md"
  "SECURITY.md"
  "CHANGELOG.md"
  "docs/architecture.md"
  "docs/threat-model.md"
  "docs/protocol.md"
  "docs/evidence-integrity.md"
  "docs/github-workflow.md"
  "docs/debugging-workflow.md"
  "docs/release-process.md"
  "docs/simulation-model.md"
  "docs/developer-console.md"
  "docs/decisions/0001-go-developer-console.md"
  "tools/tickline-dev/README.md"
)

for file in "${required_files[@]}"; do
  if [[ ! -s "$file" ]]; then
    echo "missing or empty required documentation file: $file" >&2
    exit 1
  fi
done

if grep -RIn --include='*.md' $'\r' README.md SECURITY.md CHANGELOG.md docs; then
  echo "carriage-return line endings found in Markdown files" >&2
  exit 1
fi

echo "documentation sanity check passed"
