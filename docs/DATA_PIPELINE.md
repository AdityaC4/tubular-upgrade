# Data Collection Pipeline

This note explains how the pass-order datasets are produced.

## Benchmarks and Variants
- Benchmarks live in `research_tests/`; each returns a deterministic integer.
- Expected outputs, optimization variants, and pass-order permutations are listed in `research_tests/config.json`.
- The default configuration covers 10 benchmarks × 6 variants × 6 pass orders = 360 measurements.

## Single-Run Pipeline
```bash
./scripts/collect_data.py
```
Steps executed:
1. Rebuild `build/Tubular` (unless `--skip-build` is passed).
2. Run the legacy regression suite (`./make test`, unless `--skip-tests`).
3. Execute every benchmark/variant/order combination with warm-ups and 50 timed runs.
4. Write raw rows to `artifacts/research/results.csv` and summaries to `artifacts/research/summary.json`.
Intermediate `.wat/.wasm` files appear under `artifacts/research/out/`.

Useful flags:
- `--results`, `--summary`, `--autotune-out-dir` to redirect outputs.
- `--skip-tests`, `--skip-build`, `--skip-autotune` for faster iteration.

## Repeated Runs
```bash
./scripts/repeat_collection.py --runs 3
```
Each repetition is stored in `artifacts/research/batch_runs/runN/` with its own `summary.json`, `results.csv`, and log. The script accepts `--skip-tests-after-first` and `--skip-build-after-first` to speed up subsequent runs.

## Post-processing
- `scripts/analyze_research_data.py` aggregates one or more summaries and emits CSV tables in `docs/figures/` plus `artifacts/research/aggregated_metrics.json`.
- `scripts/generate_benchmark_features_table.py` produces feature-vs-dominant-order tables (CSV + LaTeX).

## Regenerating Data
Running `./scripts/collect_data.py` overwrites the existing entries in `artifacts/research/`.
Always rerun the script after modifying benchmarks, variants, or pass orders to keep the dataset consistent with the report.
