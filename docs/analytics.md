# Investigation analytics

Tickline investigation analytics provide deterministic descriptive statistics, robust baseline comparison, explainable outlier findings, and explicit false-positive review metadata for validated investigation bundles.

The analytics layer is defensive and read-only. It does not execute commands, modify evidence, repair malformed bundles, or replace native evidence verification and replay.

## Trust boundary

The processing path is:

    verified native evidence archive
        |
        v
    deterministic investigation-bundle export
        |
        v
    strict Python bundle validation
        |
        v
    descriptive statistics
        |
        v
    verified baseline construction
        |
        v
    explainable outlier evaluation
        |
        v
    optional human review
        |
        v
    versioned JSON report

The C++ implementation remains authoritative for archive integrity, evidence-chain verification, deterministic replay, persistence, and bundle export.

Python accepts only schema-version-1 investigation bundles that satisfy the bundle structure and consistency contract.

## Input validation

The Python bundle loader rejects:

- malformed JSON;
- duplicate JSON object keys;
- missing or unexpected fields;
- unsupported schema versions;
- noncanonical unsigned decimal strings;
- unsigned values outside the 64-bit range;
- malformed SHA-256 digest text;
- malformed UTC timestamps;
- duplicate session identities;
- invalid session tick ranges;
- noncontiguous evidence ordinals;
- records that reference unknown sessions;
- discontinuous evidence chains;
- duplicate record digests;
- invalid outcome and rejection-code combinations;
- trusted-head mismatches;
- inconsistent replay counts;
- inconsistent session summaries.

Unsigned protocol and simulation values remain decimal strings in JSON and are converted to Python integers only after strict validation.

## Descriptive statistics

For each validated investigation, the analytics model computes:

- accepted, rejected, and total command counts;
- acceptance and rejection rates;
- session count;
- unique client count;
- unique target-tick count;
- first and last observed target tick;
- inclusive tick span;
- busiest target ticks;
- per-session statistics;
- per-command-type statistics;
- per-rejection-code statistics;
- per-tick statistics;
- replay verification status;
- final replay tick;
- final world fingerprint.

All ordering is deterministic. Sessions use client and session identity ordering. Command types and rejection codes use stable count-and-name ordering. Tick statistics use ascending target-tick order.

## Baseline requirements

A baseline contains statistics from multiple verified investigations.

The default policy requires at least five baseline investigations. Every baseline investigation must:

- have a verified replay result;
- have a unique archive digest;
- be independent of the candidate under evaluation.

A candidate whose archive digest is already present in the baseline is rejected. This prevents the candidate from influencing the distribution used to evaluate itself.

## Baseline metrics

The baseline evaluates these metrics:

- `command_count`;
- `rejection_rate`;
- `session_count`;
- `unique_target_ticks`;
- `tick_span`;
- `commands_per_tick`.

For each metric, the baseline stores:

- sample count;
- minimum;
- maximum;
- arithmetic mean;
- median;
- median absolute deviation;
- explicit zero-MAD tolerance.

## Outlier method

When the median absolute deviation is nonzero, Tickline uses the absolute modified z-score:

    score = abs(0.6744897501960817 * (observed - median) / MAD)

The default finding threshold is `3.5`.

When the median absolute deviation is zero, division by zero is not attempted. The policy instead uses an explicit metric-specific absolute tolerance. A finding is produced when the candidate deviation reaches or exceeds that tolerance.

Every finding records:

- metric;
- high or low direction;
- observed value;
- baseline median;
- baseline median absolute deviation;
- score;
- threshold;
- evaluation method;
- normalized severity;
- human-readable explanation.

Findings are ordered by descending normalized severity and then by metric name.

## Human review

Automated findings are not final security conclusions. Each finding may receive at most one review.

Supported dispositions are:

- `confirmed_anomaly`;
- `expected_behavior`;
- `false_positive`;
- `needs_context`.

A review requires:

- the exact finding metric;
- a supported disposition;
- a nonempty rationale;
- a nonempty reviewer identifier;
- a UTC review timestamp;
- optional unique, ascending evidence ordinals.

The review timestamp cannot be later than report generation. Evidence ordinals must refer to records available in the candidate investigation.

Example review document:

    {
      "reviews": [
        {
          "metric": "command_count",
          "disposition": "false_positive",
          "rationale": "Controlled load-test traffic.",
          "reviewer": "security-review",
          "reviewedAtUtc": "2026-07-06T12:00:00Z",
          "evidenceOrdinals": [0, 1]
        }
      ]
    }

## Command-line usage

Run the repository-local launcher from any directory:

    bash scripts/tickline-analytics.sh --help

Generate a report:

    bash scripts/tickline-analytics.sh report \
      --candidate reports/candidate.json \
      --baseline reports/baseline-01.json \
      --baseline reports/baseline-02.json \
      --baseline reports/baseline-03.json \
      --baseline reports/baseline-04.json \
      --baseline reports/baseline-05.json \
      --generated-at 2026-07-06T12:00:00Z \
      --output reports/analysis.json

Apply reviews:

    bash scripts/tickline-analytics.sh report \
      --candidate reports/candidate.json \
      --baseline reports/baseline-01.json \
      --baseline reports/baseline-02.json \
      --baseline reports/baseline-03.json \
      --baseline reports/baseline-04.json \
      --baseline reports/baseline-05.json \
      --generated-at 2026-07-06T12:00:00Z \
      --review-file reports/reviews.json \
      --output reports/analysis-reviewed.json

Omit `--output` to write the JSON report to standard output.

The baseline threshold can be configured with:

- `--minimum-samples`;
- `--modified-z-threshold`.

## Report contract

Analytics reports use schema version `1`.

Reports contain:

- deterministic generation metadata;
- investigation statistics;
- baseline policy and metric distributions;
- outlier findings;
- finding review state;
- false-positive count;
- reviewed and unreviewed finding counts.

Unsigned 64-bit identifiers, ticks, sequences, and fingerprints are emitted as decimal strings where lossless cross-language representation matters.

Floating-point report values are rounded deterministically to twelve decimal places. JSON object keys are sorted and output ends with one newline.

## Limitations

The analytics layer does not:

- prove malicious intent;
- classify a user as cheating;
- establish evidence authorship;
- verify digital signatures;
- compensate automatically for different environments or workloads;
- infer causality from correlation;
- train or execute a machine-learning model;
- replace manual investigation;
- repair invalid evidence;
- provide production monitoring or alerting.

A statistically unusual observation may be legitimate. A statistically ordinary observation may still require investigation. Findings must be interpreted using operational context and evidence review.
