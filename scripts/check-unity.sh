#!/usr/bin/env bash

set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
project_path="$repository_root/unity/TicklineForensics"
results_path="/tmp/tickline-unity-editmode-results.xml"
log_path="/tmp/tickline-unity-editmode.log"

if [[ -n "${TICKLINE_UNITY_EDITOR:-}" ]]; then
    editor="$TICKLINE_UNITY_EDITOR"
else
    editor="$(
        find "/mnt/c/Program Files/Unity/Hub/Editor" \
            -maxdepth 3 \
            -type f \
            -name Unity.exe \
            2>/dev/null \
        | sort -V \
        | tail -n 1
    )"
fi

if [[ -z "$editor" || ! -f "$editor" ]]; then
    echo "Unity editor was not found."
    echo "Set TICKLINE_UNITY_EDITOR to the full Unity.exe path."
    false
fi

rm -f "$results_path" "$log_path"

project_windows="$(wslpath -w "$project_path")"
results_windows="$(wslpath -w "$results_path")"
log_windows="$(wslpath -w "$log_path")"

echo "Unity editor: $editor"
echo "Unity project: $project_path"

"$editor" \
    -batchmode \
    -nographics \
    -projectPath "$project_windows" \
    -runTests \
    -testPlatform EditMode \
    -testResults "$results_windows" \
    -logFile "$log_windows" \
    -quit

if [[ ! -s "$results_path" ]]; then
    cat "$log_path"
    false
fi

if ! grep -Eq '<test-run[^>]+result="Passed"' "$results_path"; then
    cat "$results_path"
    cat "$log_path"
    false
fi

echo "Unity EditMode tests passed"
echo "Results: $results_path"
