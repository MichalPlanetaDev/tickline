#!/usr/bin/env bash

set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_project="$repository_root/unity/TicklineForensics"
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

windows_temp="$(
    cmd.exe /C echo %TEMP% 2>/dev/null \
        | tr -d '\r' \
        | tail -n 1
)"

if [[ -z "$windows_temp" ]]; then
    echo "Windows temporary directory could not be resolved."
    false
fi

windows_temp_wsl="$(wslpath -u "$windows_temp")"
mirror_root="$windows_temp_wsl/tickline-unity-editmode-$$"
mirror_project="$mirror_root/TicklineForensics"
mirror_results="$mirror_root/TestResults.xml"
mirror_log="$mirror_root/Unity.log"

cleanup()
{
    rm -rf -- "$mirror_root"
}

trap cleanup EXIT

rm -f -- "$results_path" "$log_path"
mkdir -p "$mirror_project"

cp -a "$source_project/Assets" "$mirror_project/"
cp -a "$source_project/Packages" "$mirror_project/"
cp -a "$source_project/ProjectSettings" "$mirror_project/"

project_windows="$(wslpath -w "$mirror_project")"
results_windows="$(wslpath -w "$mirror_results")"
log_windows="$(wslpath -w "$mirror_log")"

echo "Unity editor: $editor"
echo "Source project: $source_project"
echo "Staged project: $mirror_project"

unity_exit=0

"$editor" \
    -batchmode \
    -nographics \
    -projectPath "$project_windows" \
    -runTests \
    -testPlatform EditMode \
    -testResults "$results_windows" \
    -logFile "$log_windows" \
    || unity_exit=$?

if [[ -f "$mirror_results" ]]; then
    cp -f "$mirror_results" "$results_path"
fi

if [[ -f "$mirror_log" ]]; then
    cp -f "$mirror_log" "$log_path"
fi

if [[ "$unity_exit" -ne 0 ]]; then
    echo "Unity exited with code $unity_exit."

    if [[ -s "$log_path" ]]; then
        tail -n 160 "$log_path"
    fi

    false
fi

if [[ ! -s "$results_path" ]]; then
    echo "Unity did not produce a test-results file."

    if [[ -s "$log_path" ]]; then
        tail -n 160 "$log_path"
    fi

    false
fi

if ! grep -Eq '<test-run[^>]+result="Passed"' "$results_path"; then
    echo "Unity EditMode tests did not pass."
    cat "$results_path"

    if [[ -s "$log_path" ]]; then
        tail -n 160 "$log_path"
    fi

    false
fi

grep -m1 '<test-run' "$results_path"
echo "Unity EditMode tests passed"
echo "Results: $results_path"
echo "Log: $log_path"
