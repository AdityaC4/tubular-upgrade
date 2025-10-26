# Tubular Optimizing Compiler – Project Overview

This document summarizes the architecture, current feature set, and daily workflow for the
Tubular WebAssembly compiler. Share it with new contributors or use it as the “cover page”
for presentations and reports.

---

## 1. Goals & Scope

Tubular is a self-contained compiler for a small “Tube” language. The project serves two
purposes:

1. **Teaching / experimentation** – A readable frontend–middle–backend pipeline written in modern C++.
2. **Optimization research** – A sandbox for studying how combinations and ordering of
   classic passes (function inlining, loop unrolling, tail recursion elimination) impact
   performance. The accompanying data collection tooling supports reproducible experiments.

The compiler emits WebAssembly Text (WAT) that can be assembled to `.wasm` for execution
with `wasmtime` or Node.js.

---

## 2. Repository Layout

```
├── Tubular.cpp              # main entry point, CLI, code generation
├── src/
│   ├── frontend/            # lexer, parser, AST nodes and visitors
│   ├── middle_end/          # Control, SymbolTable, Pass interfaces, optimizations
│   ├── backend/             # WAT generator visitor
│   └── core/                # shared helpers (ASTCloner, tools)
├── tests/                   # legacy regression suites + browser harness artifacts
├── research_tests/          # curated workloads for pass-order experiments
├── autotuning/              # grid-search runner and configs (legacy)
├── scripts/collect_data.py  # one-command build/test/experiment pipeline
└── artifacts/research/      # generated data (results.csv, summary.json, .wat/.wasm)
```

Key documentation:

- `README.md` – quick-start instructions and high-level feature list.
- `SPECS.md` – Tube language grammar and semantics.
- `TESTING.md` – legacy regression strategy.
- `docs/PROJECT_OVERVIEW.md` – (this file) architectural summary.
- `docs/DATA_PIPELINE.md` – data-collection workflow.

---

## 3. Architectural Highlights

### 3.1 Frontend

- **Lexer** generated from `lexer.emplex`, producing tokens consumed by `TokenQueue`.
- **Parser** lives in `Tubular::Parse`, building a typed AST using the node classes in `src/frontend/ASTNode.hpp`.
- **Symbol management** via `SymbolTable` and `Control` (tracks locals, labels, WAT emission context).

### 3.2 Middle-end

- Pass infrastructure (`Pass`, `PassManager`, `ASTCloner`) enables quick experimentation.
- Optimizations:
  - `FunctionInliningPass` – inlines small, pure functions; conservatively avoids unsafe cases.
  - `LoopUnrollingPass` – targets affine while-loops with literal bounds/inc; clones bodies and emits remainder loops.
  - `TailRecursionPass` – introduces `ASTNode_TailCallLoop` nodes to convert tail calls into loops.
- Pass **ordering is configurable** via a CLI flag (`--pass-order=inline,unroll,tail`). This is central to current research.

### 3.3 Backend

- `WATGenerator` traverses the AST using the visitor pattern and emits WAT line-by-line through `Control`.
- Helper runtime functions (string ops, memory management) are emitted directly from `Tubular::ToWAT`.

---

## 4. CLI & Flags

```
./build/Tubular program.tube [options]

Options:
  --no-unroll
  --unroll-factor=N
  --no-inline
  --tail=loop|off
  --pass-order=inline,unroll,tail  # any permutation of the three passes
```

Examples:

- Default build: `./build/Tubular tests/test-01.tube > out.wat`
- Custom ordering: `./build/Tubular research_tests/rt05_nested_mix.tube --unroll-factor=4 --pass-order=tail,inline,unroll`

---

## 5. Testing Strategy

1. **Legacy regression (`./make test`)**
   - 24 “Project 4” language tests + 30 “Project 3” compatibility programs.
   - 30+ error tests (ensures compile-time diagnostics still fire).
   - Optimization-specific harnesses:
     - Loop unrolling generator.
     - Function inlining Node-based checker.
     - Tail recursion Node benchmark (detects stack overflows vs loopified success).
   - CLI flag combination sanity checks.

2. **Research test suite (`research_tests/`)**
   - Ten workloads (fib, factorial, loop summation, string growth, branchy loops, nested loops, etc.) designed to expose ordering interactions.
   - Each program returns a deterministic integer; expectations stored in `research_tests/config.json`.
   - Driven by the new data pipeline (see next section).

---

## 6. Build & Tooling

- **Build:** `./make` (wraps CMake; builds `build/Tubular`).
- **Clean:** `./make clean`, `./make clean-test`, `./make clean-inline`, etc.
- **Autotuning (legacy):** `python3 autotuning/run_autotune.py --config autotuning/config/default.json ...`
- **Research pipeline:** `./scripts/collect_data.py` (details in `docs/DATA_PIPELINE.md`).

Dependencies:

- C++20 compiler + CMake ≥ 3.16.
- `wat2wasm` (wabt) and Node.js for execution tests.
- Python 3.8+ for the autotuner/data scripts.

---

## 7. Current Status & Roadmap

- **Stable:** parser, type system, WAT emission, baseline optimizations.
- **Research-ready:** pass ordering flag, curated benchmarks, automated data collection.
- **Potential next steps:**
  - Additional passes (CSE, DCE, const folding).
  - Pass-order search heuristics (ML-guided or heuristic ranking).
  - Richer benchmarking (larger workloads, Wasmtime profiling).
  - Visualization dashboards built on `artifacts/research/summary.json`.

---

## 8. Onboarding Checklist

1. Install dependencies (`wabt`, Node.js, Python).
2. Build once: `./make`.
3. Verify regression suite: `./make test`.
4. Run the research pipeline: `./scripts/collect_data.py`.
5. Inspect `artifacts/research/summary.json` to understand pass-order effects.
6. Read `research_tests/*.tube` to learn benchmark structure.

With that baseline, you can safely modify passes, add new benchmarks, or explore alternative pass orderings while preserving reproducibility. Welcome to Tubular!
