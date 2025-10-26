# Autotuning Workflow

The autotuning tooling automates the generation and benchmarking of WebAssembly
artifacts produced by the Tubular compiler. It compiles a suite of benchmarks
with various optimization configurations, executes the resulting binaries via
Node.js, and writes a CSV summary that can be imported into notebooks or
analysis scripts.

## Requirements

- `build/Tubular` (built via `./make`)
- [`wat2wasm`](https://github.com/WebAssembly/wabt)
- [Node.js](https://nodejs.org/) (used to execute the generated Wasm modules)
- Python 3.8+

## Benchmarks

Benchmarks are stored in `tests/benchmarks/` and are designed to finish within a
few milliseconds. Each benchmark returns an integer result so the autotuner can
verify correctness across optimization variants.

## Configuration

Configurations are JSON files (see `autotuning/config/default.json`) with:

```json
{
  "runs": 5,
  "warmup_runs": 1,
  "benchmarks": [
    { "name": "fib", "path": "tests/benchmarks/bench-fib.tube", "expected": 6765 }
  ],
  "variants": [
    { "name": "baseline", "flags": [] },
    { "name": "unroll-4", "flags": ["--unroll-factor=4"] }
  ]
}
```

- `runs`: number of timed executions per benchmark/variant pair.
- `warmup_runs`: executions discarded from timing (useful for JIT warm-up).
- `benchmarks`: list of benchmark descriptors (`path` must be relative to repo
  root; `expected` is optional but recommended).
- `variants`: optimization flag sets to test.

## Running

```bash
./autotuning/run_autotune.py \
  --config autotuning/config/default.json \
  --output autotuning/results.csv
```

Optional flags:

- `--runs` / `--warmup` to override config values.
- `--tubular`, `--wat2wasm`, `--node` to point at tool binaries.
- `--out-dir` to change the directory used for generated WAT/WASM artifacts.

The script prints per-run summaries and writes a CSV containing quartiles,
artifact sizes, and the observed results. Warm-up runs are executed but not
included in the CSV timings.

## Workflow

1. Ensure `wat2wasm` and `node` are installed and on `PATH`.
2. Build the compiler: `./make`.
3. Adjust or create JSON configs in `autotuning/config/`.
4. Run the autotuner (see above).
5. Analyse `autotuning/results.csv` in your preferred notebook or plotting tool.

The default configuration exercises baseline vs. several optimization flag
combinations on the bundled benchmark suite. Feel free to duplicate the file
and tailor it to the workloads you care about.
