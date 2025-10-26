# Tubular Data Collection Pipeline

This guide explains how to reproduce the datasets used in the pass-order study:
what the research benchmarks are, how the `collect_data.py` script works, and how
to interpret the generated artifacts.

---

## 1. Goals

We want a single command that:

1. Builds the current compiler.
2. (Optionally) re-runs the legacy regression suites for sanity.
3. Compiles and executes each curated research benchmark for *every*
   optimization variant and pass-order permutation.
4. Verifies correctness via Node.js.
5. Records timing statistics and summarized comparisons suitable for plots or reports.

The output should be deterministic and self-contained so that future readers can
validate results without manual intervention.

---

## 2. Research Benchmarks

Located in `research_tests/`, these programs are designed to exhibit different
interactions between inlining, unrolling, and tail recursion. Highlights:

| Name | Description | Expected Result |
| ---- | ----------- | --------------- |
| `rt01-fib-recursive` | Naïve recursive Fibonacci (`Fib(25)`) | 75\u202f025 |
| `rt02-tail-factorial` | Tail-recursive factorial (`12!`) | 479\u202f001\u202f600 |
| `rt03-loop-summation` | Sum 1..20\u202f000 | 200\u202f010\u202f000 |
| `rt04-stride-heavy-loop` | Loop with helper `Mix` | 24\u202f450\u202f418 |
| `rt05-nested-mix` | 120×120 nested loops | 36\u202f414 |
| `rt06-string-wrapping` | Iteratively wrap strings | 28 |
| `rt07-helper-inline` | Helper-heavy worker loop | 767\u202f979 |
| `rt08-branchy-loop` | Loop with `continue` and conditionals | 12\u202f342 |
| `rt09-matrix-mix` | Matrix-style double loop with helper | 9\u202f992\u202f080 |
| `rt10-control-baseline` | Modular arithmetic stress | 282\u202f763 |

The manifest `research_tests/config.json` stores the expected value, the six
optimization variants, and the six pass-order permutations for all runs.

Variants (flags):

1. `baseline` – defaults (all passes enabled, unroll factor 4).
2. `no-inline` – `--no-inline`.
3. `unroll-4` – `--unroll-factor=4`.
4. `unroll-8` – `--unroll-factor=8`.
5. `tail-off` – `--tail=off`.
6. `combo-inline-unroll` – `--unroll-factor=4 --no-inline`.

Pass orders (CLI `--pass-order=`):

1. `inline-unroll-tail`
2. `inline-tail-unroll`
3. `unroll-inline-tail`
4. `unroll-tail-inline`
5. `tail-inline-unroll`
6. `tail-unroll-inline`

This yields **10 benchmarks × 6 variants × 6 orders = 360 runs** per dataset.

---

## 3. Data Collection Script

`./scripts/collect_data.py` orchestrates the entire workflow. Usage:

```bash
# Full pipeline (build + regression sanity + data sweep)
./scripts/collect_data.py

# Skip expensive components while iterating:
./scripts/collect_data.py --skip-tests        # Skip ./make test
./scripts/collect_data.py --skip-build        # Reuse existing build/Tubular
./scripts/collect_data.py --skip-autotune     # Only rebuild/tests
```

Key arguments:

| Flag | Description | Default |
| ---- | ----------- | ------- |
| `--project-root` | Repository root | `.` |
| `--config` | Autotuner config | `research_tests/config.json` |
| `--results` | Output CSV | `artifacts/research/results.csv` |
| `--summary` | Summary JSON | `artifacts/research/summary.json` |
| `--autotune-out-dir` | Intermediate `.wat/.wasm` dir | `artifacts/research/out/` |

### 3.1 Steps executed

1. `./make` – ensures `build/Tubular` is current.
2. `./make test` (unless `--skip-tests`) – runs the legacy regression suite for sanity:
   - 24 Project 4 tests, 30 Project 3 tests, error tests.
   - Loop-unrolling artifact generation.
   - Function inlining + tail recursion Node-based checks.
   - CLI flag validation.
3. `python autotuning/run_autotune.py ...` – but using the research config:
   - For each benchmark/variant/order:
     - Compile `.tube → .wat`.
     - Assemble `.wat → .wasm`.
     - Run via Node.js; compare return value to manifest.
     - Record median, p25, p75 timings (ms) alongside wat/wasm sizes.
   - Artifacts saved in `artifacts/research/out/<benchmark>__<variant>__<order>.wat/.wasm`.
4. Parse the CSV, compute best/worst orders per variant, and emit `summary.json`.
5. Print a console digest (top 10 deltas + pass-order aggregates).

---

## 4. Output Files

### 4.1 `artifacts/research/results.csv`

Raw data – each row includes:

- `benchmark`, `variant`, `pass_order`, `flags`
- `wat_size`, `wasm_size`
- `runs`, `warmup_runs`
- `p25_ms`, `median_ms`, `p75_ms`
- `result` (stringified return value; guaranteed to match expected)

Use this file for custom plotting or statistical analysis.

### 4.2 `artifacts/research/summary.json`

Derived statistics:

- `variant_stats`: for each `(benchmark, variant)` pair:
  - `best_order`, `best_median_ms`, `best_flags`
  - `worst_order`, `worst_median_ms`, `worst_flags`
  - `delta_ms`, `delta_pct` (percentage difference relative to worst)
- `pass_order_stats`: aggregate mean/min/max across all 60 samples per ordering.

This file is ready to be consumed by plotting scripts or embedded directly in a
report to highlight quantitative findings (e.g., “`rt06-string-wrapping` regresses by
7.3 % when ordering is inline→unroll→tail”).

---

## 5. Example Findings (2025‑10‑26 run)

- `rt01-fib-recursive/baseline`: `tail-unroll-inline` = 20.74 ms vs `inline-unroll-tail` = 22.30 ms (7.0 % faster).
- `rt06-string-wrapping/tail-off`: `tail-inline-unroll` = 19.67 ms vs `inline-unroll-tail` = 21.21 ms (7.25 % faster).
- `rt04-stride-heavy-loop/combo-inline-unroll`: difference of 1.48 ms (7.15 %) purely from ordering.
- Converging the pass-order stats across all 360 runs reveals a consistent bias:
  inline→unroll→tail averages 20.18 ms; tail→unroll→inline averages 20.26 ms.

Running the script again will overwrite the artifacts with fresh data, preserving the same structure
for reproducibility.

---

## 6. Tips for Report Preparation

- Use `summary.json` as the “source of truth” for best/worst comparisons.
- When plotting, normalize the median times to the best ordering per variant to show relative regressions.
- Include at least one absolute-time plot (in ms) so readers can map percentages to real costs.
- Keep the script output in your appendix/log for reproducibility (the `generated_at` timestamp is embedded).

---

## 7. Extending the Pipeline

- **Adding benchmarks:** drop a `.tube` file in `research_tests/`, update `config.json`
  with its path and expected result.
- **Changing variants:** edit the `variants` list in `config.json`.
- **Alternate pass orders:** add/remove entries in `pass_orders`.
- **Custom output dirs:** use `--results`, `--summary`, `--autotune-out-dir` flags to
  keep multiple experiment runs side-by-side.

Always rerun `./scripts/collect_data.py --skip-tests` (or full run) after changes to regenerate artifacts.

### 3.2 Batch runs for consistency checks

Use `./scripts/repeat_collection.py --runs 3` to run the full pipeline multiple times.
Each execution lands in `artifacts/research/batch_runs/runN/` (own results, summary,
log, and intermediate `.wat/.wasm`), and a `manifest.json` records top deltas per run.
Add `--skip-tests-after-first` or `--skip-build-after-first` if you only need one full
sanity pass.
