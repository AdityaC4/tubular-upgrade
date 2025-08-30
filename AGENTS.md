# Repository Guidelines

## Project Structure & Module Organization
- `Tubular.cpp`: main compiler entrypoint.
- `src/frontend`: lexer, tokens, AST (`lexer.hpp`, `ASTNode.hpp`, `TokenQueue.hpp`).
- `src/middle_end`: optimization passes and types (`LoopUnrollingPass.hpp`, `FunctionInliningPass.hpp`, `TailRecursionPass.hpp`, `PassManager.hpp`).
- `src/backend`: code generation (`WATGenerator.hpp`).
- `src/core`: shared helpers (`tools.hpp`).
- `tests/`: `.tube` cases, runners, and perf suites (`run_tests.sh`, `loop-unrolling/`, `function-inlining/`).
- `build/`: CMake outputs (git-ignored).

## Build, Test, and Development Commands
- `./build.sh [release|debug|grumpy] [clean] [tests]`: configure + build (use this first).
- `./make`: build using existing CMake cache in `build/`.
- `./make test`: run standard, loop-unrolling, and inlining tests.
- `./make clean | clean-test | clean-unroll | clean-inline`: targeted cleans.
- CMake directly: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build`.
- Dependency: tests use `wat2wasm` (install WABT) for WAT→WASM.

## Coding Style & Naming Conventions
- Language: C++20. Format with clang-format (LLVM base, 2 spaces, 120 cols, sorted includes).
- Run formatting: `clang-format -i $(git ls-files '*.cpp' '*.hpp')` before committing.
- Naming: types/classes PascalCase; functions/methods Capitalized_With_Underscores (e.g., `Parse_UnaryTerm`); constants UPPER_SNAKE_CASE; keep file names consistent with existing headers (CamelCase or snake_case as present).
- Prefer RAII and `std::unique_ptr`; avoid raw new/delete; keep includes local (`src/...`) as in current code.

## Testing Guidelines
- Full suite: `./make test` or `cd tests && ./run_tests.sh`.
- Test files: `test-XX.tube`, legacy `P3-test-XX.tube`; error tests: `test-error-XX.tube`, `P3-test-error-XX.tube`.
- Perf suites: `tests/loop-unrolling/run_unroll_tests.sh` and `tests/function-inlining/run_inlining_tests.sh`.
- Browser perf demo: `cd tests && python -m http.server` then open `loop-unrolling/loop-unrolling-tester.html`.

## Commit & Pull Request Guidelines
- Commits: imperative, concise; optional scope prefix (e.g., `middle_end: add LoopUnrollingPass`); one logical change per commit.
- PRs: clear description, rationale, and testing evidence (command outputs or screenshots for browser tests). Link issues, keep diffs focused, update docs (`README.md`, `TESTING.md`) and add/adjust tests when changing compiler behavior.

## Architecture Notes
- Pipeline: Frontend (lex/parse/AST) → Middle-End (passes via `PassManager`) → Backend (`WATGenerator`) emitting WAT; tests convert WAT→WASM with WABT.

## Upcoming Optimizations & Autotuner Plan
- Tail recursion: broaden `TailRecursionPass` coverage (mutual/simple args), add CLI toggle (planned `--tail=loop|off`) and new tests under `tests/tail-recursion/`.
- Tunable knobs: `--unroll-factor=N` (1 disables), `--no-inline`, and planned `--tail=…` exposed for search.
- Autotuner (planned `tools/autotune.py`): grid-search small configs (inline on/off × unroll {1,4,8,16} × tail {loop,off}); for each config: build → compile `.tube` → `wat2wasm` → time execution; fallback to compile-time if runtime unavailable.
- Integration: add CMake target `tune` to run the autotuner over a benchmark set in `tests/benchmarks/` and write results to `tests/autotune/results/`.
- PR readiness: any change impacting knobs must update docs, add tests, and keep default behavior passing `./make test`.
