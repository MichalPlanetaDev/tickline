#!/usr/bin/env bash
set -Eeuo pipefail

repository_root="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." &&
    pwd
)"

manifest_path="$repository_root/scripts/checks/manifest.tsv"

fail()
{
    printf 'check-local: %s\n' "$*" >&2
    exit 2
}

[[ -f "$manifest_path" ]] ||
    fail "manifest not found: $manifest_path"

version_seen=false
stage_count=0
started_at=$SECONDS

printf '%s\n\n' "Tickline local verification"

while IFS=$'\t' read -r \
    record_type \
    field_1 \
    field_2 \
    field_3 \
    field_4 \
    field_5 ||
    [[ -n "${record_type:-}" ]]
do
    record_type="${record_type%$'\r'}"

    if [[ -z "$record_type" || "$record_type" == \#* ]]; then
        continue
    fi

    case "$record_type" in
        version)
            if [[ "$version_seen" == true ]]; then
                fail "manifest contains multiple version records"
            fi

            [[ "$field_1" == "1" ]] ||
                fail "unsupported manifest version: $field_1"

            version_seen=true
            ;;

        stage)
            [[ "$version_seen" == true ]] ||
                fail "stage record appears before manifest version"

            stage_id="$field_1"
            stage_label="$field_2"
            script_path="$field_3"
            enabled_by_default="$field_4"

            case "$enabled_by_default" in
                true)
                    ;;
                false)
                    continue
                    ;;
                *)
                    fail \
                        "stage $stage_id has invalid enabled value: $enabled_by_default"
                    ;;
            esac

            absolute_script="$repository_root/$script_path"

            [[ -f "$absolute_script" ]] ||
                fail "stage $stage_id script not found: $script_path"

            [[ -x "$absolute_script" ]] ||
                fail "stage $stage_id script is not executable: $script_path"

            stage_count=$((stage_count + 1))
            stage_started_at=$SECONDS

            printf '[%d] %s\n' "$stage_count" "$stage_label"

            if "$absolute_script"; then
                stage_duration=$((SECONDS - stage_started_at))

                printf \
                    '    passed in %ds\n\n' \
                    "$stage_duration"
            else
                exit_code=$?
                stage_duration=$((SECONDS - stage_started_at))

                printf \
                    '    failed in %ds with exit code %d\n' \
                    "$stage_duration" \
                    "$exit_code" \
                    >&2

                exit "$exit_code"
            fi
            ;;

        *)
            fail "unknown manifest record type: $record_type"
            ;;
    esac
done < "$manifest_path"

[[ "$version_seen" == true ]] ||
    fail "manifest does not contain a version record"

((stage_count > 0)) ||
    fail "manifest contains no enabled stages"

total_duration=$((SECONDS - started_at))

printf '%s\n' "Result: passed"
printf 'Checks: %d passed\n' "$stage_count"
printf 'Total: %ds\n' "$total_duration"
