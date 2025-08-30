# Autotuner Readiness & Pass Audit

## Summary
This repository can support an autotuner with small but important changes. Unroll factor and inlining are surfaced via flags, but CLI parsing and pass robustness need tweaks. Tail recursion exists as a stub and should be toggleable to become a tuning knob.

## Autotuner Feasibility
- Knobs present: `--unroll-factor=N`, `--no-unroll`, `--no-inline` (Tubular.cpp).
- Gaps blocking tuning combos:
  - CLI only accepts one flag; cannot tune combinations (e.g., `--unroll-factor=8 --no-inline`).
  - Tail recursion pass (`TailRecursionPass`) is a no‑op and not toggleable.
- Orchestration: Tests already compile `.tube`→WAT and use `wat2wasm`; timing hooks exist in `tests/loop-unrolling/run_unroll_tests.sh`. Execution timing via Node is optional in inlining tests.
- Pass order: Fixed (Inlining → Unrolling → Tail recursion). Exposing order would expand search space later.

## Pass Audit (Issues & Risks)
- FunctionInliningPass (`src/middle_end/FunctionInliningPass.hpp`)
  - Fragile introspection: parses IDs/operators from `GetTypeName()` strings. Prefer explicit getters (e.g., `ASTNode_Function::GetFunId`, `GetParamIds`).
  - Incomplete cloner: literals (float/string) cloning relies on `GetTypeName()` which doesn’t carry values; many node kinds not handled (e.g., `FunctionCall`, `If`).
  - Semantics: no side‑effect analysis; risk of duplicating work; no symbol table updates for temps if introduced.
  - Heuristic: only counts body statements; lacks guards for recursion/complex control flow.
- LoopUnrollingPass (`src/middle_end/LoopUnrollingPass.hpp`)
  - Analysis narrow: detects only `var = var +/- int` and simple comparisons; skips nuanced bounds and complex bodies.
  - Cloning gaps: internal `ASTCloner` handles only `Math2`, `Var`, `IntLit`, `Block`; drops other node kinds silently.
  - Condition adjust: handles `<, <=, >, >=`; leaves `==/!=` (unsafe); limited validation of monotonicity.
  - Correctness edge: induction substitution may be wrong if loop var is reassigned in body beyond the recognized increment.
- TailRecursionPass (`src/middle_end/TailRecursionPass.hpp`)
  - Stub: no transformation implemented; not toggleable from CLI.

## Recommended Changes (to enable tuning)
- CLI
  - Parse multiple flags (`argv[2..]`) to allow combined knobs.
  - Add `--tail=loop|off` to control `TailRecursionPass`.
  - Optional: `--pass-order=inline,unroll,tail` for future search.
- Inliner
  - Replace `GetTypeName()` parsing with node APIs; add literal getters if needed.
  - Implement a robust AST cloner or visitor covering at least `Var/Int/Float/String/Math1/Math2/FunctionCall/If/Block/Return`.
  - Inline only small, side‑effect‑free functions; preserve semantics; update `var_ids` as needed.
- Unroller
  - Expand cloner coverage (If/Return/FunctionCall/While) or conservatively bail out.
  - Tighten safety: forbid `==/!=`, validate single assignment to loop var, and check monotonic bounds.
- Tail recursion
  - Implement basic self‑tail call loopification and guard with `--tail` flag.

## Suggested Implementation Order
1) Multi‑flag CLI + `--tail` toggle. 2) Inliner safety/clone fixes. 3) Unroller cloning + checks. 4) Minimal tail‑rec loopification. 5) Add `tools/autotune.py` with grid search over knobs.

## Next Steps Checklist
- [ ] Allow multiple CLI flags; add `--tail=loop|off`.
- [ ] Harden inliner (APIs, cloner, heuristics, tests).
- [ ] Harden unroller (cloner coverage, safety checks, tests).
- [ ] Implement tail recursion loopification and tests.
- [ ] Add autotuner script and CMake `tune` target.
