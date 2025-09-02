# Tubular Compiler - Project Completion Issues

This document outlines the comprehensive set of GitHub issues needed to complete the Tubular WebAssembly compiler project and transform it into a research-grade autotuning prototype, following the roadmap outlined in [PLAN.md](PLAN.md).

## 🎯 Project Overview

The Tubular compiler is currently a well-structured WebAssembly compiler with working optimization passes. The goal is to implement an autotuning framework that can systematically explore optimization combinations and deliver measurable performance improvements.

**Current Status:**
- ✅ Modular architecture with frontend/middle-end/backend separation
- ✅ Working optimization passes (function inlining, loop unrolling)
- ✅ Comprehensive test suite (84 tests total)
- ✅ WebAssembly toolchain integration (wat2wasm, wasmtime)
- ⚠️ CLI parsing needs multi-flag support for autotuning
- ⚠️ Optimization passes need robustness improvements
- ❌ Autotuning framework missing
- ❌ Research documentation incomplete

## 📊 Priority Matrix

### Phase 1: Critical Infrastructure (Week 1)
| Issue | Priority | Description | Dependencies |
|-------|----------|-------------|--------------|
| #1 | 🔴 HIGH | Fix CLI multi-flag parsing | None |
| #2 | 🔴 HIGH | Harden FunctionInliningPass | None |
| #3 | 🔴 HIGH | Complete LoopUnrollingPass | None |
| #4 | 🔴 HIGH | Implement TailRecursionPass | None |
| #5 | 🟡 MED | Setup dependency documentation | None |

### Phase 2: Autotuning Core (Week 2)
| Issue | Priority | Description | Dependencies |
|-------|----------|-------------|--------------|
| #6 | 🔴 HIGH | Create benchmark suite | None |
| #7 | 🔴 HIGH | Implement autotuner framework | #1, #6 |
| #8 | 🟡 MED | Add performance measurement tools | #7 |
| #9 | 🟡 MED | Add unit tests for passes | #2, #3, #4 |

### Phase 3: Research & Analysis (Week 3)
| Issue | Priority | Description | Dependencies |
|-------|----------|-------------|--------------|
| #10 | 🔴 HIGH | Performance analysis & tech report | #7, #8 |
| #11 | 🟡 MED | OpenTuner integration | #7 |
| #12 | 🟡 MED | Enhance integration testing | #9 |
| #13 | 🟢 LOW | Add comprehensive API docs | Various |

### Phase 4: Polish & Release (Week 4)
| Issue | Priority | Description | Dependencies |
|-------|----------|-------------|--------------|
| #14 | 🟡 MED | Create release documentation | #10 |
| #15 | 🟡 MED | Add GitHub Actions CI/CD | #12 |
| #16 | 🟢 LOW | Enhance build system | None |
| #17 | 🟢 LOW | Add advanced optimization passes | All |

## 📝 Detailed Issue Specifications

### 🔧 Infrastructure Issues

#### Issue #1: Fix CLI Multi-Flag Parsing Support
**Type:** Bug/Enhancement | **Priority:** 🔴 HIGH | **Effort:** 1-2 days

**Problem:** Current CLI only accepts single optimization flags, but autotuning requires combinations like `--unroll-factor=8 --no-inline --tail=off`.

**Solution:**
- Modify argument parsing in `Tubular.cpp` to handle multiple flags
- Add validation for flag combinations
- Support all flags simultaneously: `--unroll-factor=N`, `--no-unroll`, `--no-inline`, `--tail=loop|off`

**Acceptance Criteria:**
- [ ] CLI accepts multiple flags in any order
- [ ] Invalid combinations show helpful error messages
- [ ] All existing single-flag functionality preserved
- [ ] Add tests for flag combinations

---

#### Issue #2: Harden FunctionInliningPass Robustness
**Type:** Bug/Code Quality | **Priority:** 🔴 HIGH | **Effort:** 2-3 days

**Problem:** Current implementation has fragile introspection and incomplete AST cloning (identified in AUTOTUNER_FEASIBILITY.md).

**Critical Issues:**
- Parses function IDs from `GetTypeName()` strings instead of proper APIs
- Incomplete cloner missing many node types (FunctionCall, If, etc.)
- No side-effect analysis - risk of duplicating work
- Weak heuristics (only counts statements)

**Solution:**
- Replace string parsing with proper node APIs (`ASTNode_Function::GetFunId()`)
- Implement complete AST cloner covering all node types
- Add side-effect analysis for safe inlining decisions
- Improve heuristics with recursion/complexity guards
- Update symbol table for introduced temporaries

**Acceptance Criteria:**
- [ ] No string parsing for node introspection
- [ ] Complete AST cloner handles all node types
- [ ] Side-effect analysis prevents unsafe inlining
- [ ] Enhanced heuristics with safety guards
- [ ] All existing tests pass
- [ ] New tests for edge cases

---

#### Issue #3: Complete LoopUnrollingPass Implementation
**Type:** Bug/Enhancement | **Priority:** 🔴 HIGH | **Effort:** 2-3 days

**Problem:** Current implementation has narrow analysis scope and incomplete cloning.

**Issues:**
- Only detects `var = var +/- int` patterns
- Internal cloner only handles limited node types
- Leaves `==/!=` conditions (unsafe)
- Limited validation of loop monotonicity

**Solution:**
- Expand analysis for complex increment patterns
- Implement complete AST cloner for all node types
- Add safety checks for `==/!=` conditions
- Validate single assignment to loop variables
- Improve remainder loop generation

**Acceptance Criteria:**
- [ ] Broader loop pattern recognition
- [ ] Complete AST cloner coverage
- [ ] Safety validation for all condition types
- [ ] Monotonicity checks implemented
- [ ] All existing tests pass
- [ ] Performance improvements maintained

---

#### Issue #4: Implement TailRecursionPass Functionality
**Type:** Enhancement | **Priority:** 🔴 HIGH | **Effort:** 2-3 days

**Problem:** Currently a stub implementation with no actual transformation.

**Solution:**
- Implement tail recursion to loop conversion
- Add CLI flag support: `--tail=loop|off`
- Handle simple tail recursive patterns
- Preserve function semantics while improving performance

**Implementation:**
- Detect tail recursive function calls
- Transform recursive calls into while loops
- Handle accumulator-style tail recursion
- Maintain correctness with parameter management

**Acceptance Criteria:**
- [ ] Functional tail recursion optimization
- [ ] CLI flag `--tail=loop|off` working
- [ ] Tests showing performance improvement
- [ ] Create `tests/tail-recursion/` test suite
- [ ] Documentation for the optimization

---

#### Issue #5: Add Dependency Setup Documentation
**Type:** Documentation | **Priority:** 🟡 MED | **Effort:** 1 day

**Problem:** Project requires WebAssembly toolchain but setup isn't documented.

**Solution:**
- Create setup script for wabt and wasmtime installation
- Add comprehensive dependency documentation to README
- Consider Docker containerization for development
- Update CI/CD pipeline dependencies

**Acceptance Criteria:**
- [ ] Setup script for all platforms
- [ ] Updated README with dependency instructions
- [ ] Docker option for easy development setup
- [ ] CI/CD has all required dependencies

---

### 🚀 Autotuning Framework Issues

#### Issue #6: Create Benchmark Suite for Autotuning
**Type:** Enhancement | **Priority:** 🔴 HIGH | **Effort:** 2-3 days

**Problem:** Need dedicated benchmarks for optimization measurement per PLAN.md.

**Requirements:**
- 5 core benchmarks: Fibonacci(40), factorial(15), 1000-iteration loop, string concatenation, array sum
- Each <5ms baseline for rapid testing
- Located in `tests/benchmarks/` directory
- Suitable for autotuning experiments

**Solution:**
- Design benchmark programs in Tubular language
- Create measurement infrastructure
- Add baseline timing collection
- Ensure programs exercise different optimization opportunities

**Acceptance Criteria:**
- [ ] 5 benchmark programs implemented
- [ ] Each runs in <5ms baseline
- [ ] `tests/benchmarks/` directory structure
- [ ] Documentation for each benchmark
- [ ] Integration with testing infrastructure

---

#### Issue #7: Implement Core Autotuner Framework
**Type:** Enhancement | **Priority:** 🔴 HIGH | **Effort:** 3-4 days

**Problem:** Main goal from PLAN.md - need autotuning system to explore optimization combinations.

**Requirements:**
- Grid search across optimization combinations
- Configuration space: inline (on/off) × unroll (1,4,8,16) × tail (loop,off)
- JSON configuration format
- Timing measurement with statistical analysis
- Correctness validation for all configurations

**Implementation:**
- Create `tools/autotune.py` script
- JSON configuration system
- Compile → time → validate pipeline
- Statistical analysis (median of 30 runs)
- Results reporting and visualization

**Acceptance Criteria:**
- [ ] `tools/autotune.py` with grid search
- [ ] JSON configuration format
- [ ] Timing measurement infrastructure
- [ ] Correctness validation for all configs
- [ ] Results reporting and analysis
- [ ] CMake target `make tune`

---

#### Issue #8: Add Performance Measurement Tools
**Type:** Enhancement | **Priority:** 🟡 MED | **Effort:** 2 days

**Problem:** Need robust timing and analysis infrastructure for autotuning.

**Solution:**
- Implement baseline timing using `time.perf_counter`
- Add compilation time measurement
- WebAssembly execution timing via wasmtime
- Statistical analysis with multiple runs
- Performance comparison reporting

**Metrics to Track:**
- Speed-up vs baseline (target ≥1.25×)
- Compile-time cost (target <2× baseline)
- Code size delta (WebAssembly file size)
- Success rate of optimizations

**Acceptance Criteria:**
- [ ] Baseline timing infrastructure
- [ ] Compilation time measurement
- [ ] WebAssembly execution timing
- [ ] Statistical analysis (30 runs median)
- [ ] Performance comparison reports
- [ ] Integration with autotuner

---

### 🧪 Testing & Quality Issues

#### Issue #9: Add Unit Tests for Optimization Passes
**Type:** Testing | **Priority:** 🟡 MED | **Effort:** 2-3 days

**Problem:** Need comprehensive unit testing for pass infrastructure.

**Solution:**
- Create unit tests for PassManager
- Individual pass testing (inlining, unrolling, tail recursion)
- Test edge cases and error conditions
- Performance regression tests

**Test Coverage:**
- PassManager functionality
- Individual pass correctness
- AST cloning operations
- Optimization combination effects
- Edge cases and error handling

**Acceptance Criteria:**
- [ ] Unit tests for PassManager
- [ ] Individual pass test suites
- [ ] Edge case coverage
- [ ] Performance regression detection
- [ ] Integration with build system

---

#### Issue #10: Performance Analysis & Technical Report
**Type:** Research/Documentation | **Priority:** 🔴 HIGH | **Effort:** 3-4 days

**Problem:** Need research-grade analysis and documentation per PLAN.md.

**Deliverables:**
- 4-page technical report
- Performance data collection and analysis
- Speed-up measurements across benchmarks
- Compile-time and code-size analysis
- Academic-quality results presentation

**Report Structure:**
- Problem: Hand-crafted heuristics vs autotuning
- Method: Grid/ensemble search over optimization knobs
- Results: Performance tables and visualizations
- Discussion: Future work and ML integration opportunities

**Acceptance Criteria:**
- [ ] Complete performance data collection
- [ ] 4-page technical report
- [ ] Academic-quality analysis
- [ ] Performance visualizations
- [ ] Future research directions

---

### 🔧 Code Quality & Documentation Issues

#### Issue #11: OpenTuner Integration (Stretch Goal)
**Type:** Enhancement | **Priority:** 🟡 MED | **Effort:** 3-4 days

**Problem:** Advanced search beyond grid search for better optimization discovery.

**Solution:**
- Integrate OpenTuner Python API
- Ensemble search heuristics
- Machine learning guided optimization
- Comparative analysis vs grid search

**Benefits:**
- Better optimization discovery with <50 runs
- Academic credibility with established tool
- Advanced search algorithms
- Research publication potential

**Acceptance Criteria:**
- [ ] OpenTuner integration working
- [ ] Ensemble search implemented
- [ ] Comparative analysis vs grid search
- [ ] Documentation for advanced features

---

#### Issue #12: Enhance Integration Testing
**Type:** Testing | **Priority:** 🟡 MED | **Effort:** 2 days

**Problem:** Need comprehensive testing of optimization combinations.

**Solution:**
- Test all optimization combinations
- Validate correctness across optimization levels
- Browser-based performance validation
- Automated performance benchmarking

**Acceptance Criteria:**
- [ ] All optimization combinations tested
- [ ] Correctness validation suite
- [ ] Browser performance tests
- [ ] Automated benchmarking in CI

---

#### Issue #13: Add Comprehensive API Documentation
**Type:** Documentation | **Priority:** 🟢 LOW | **Effort:** 2-3 days

**Problem:** Need developer documentation for extending the compiler.

**Solution:**
- Document all public APIs
- Usage examples for optimization passes
- Developer guide for compiler extension
- Architecture documentation

**Acceptance Criteria:**
- [ ] Complete API documentation
- [ ] Usage examples and tutorials
- [ ] Developer extension guide
- [ ] Architecture documentation

---

### 🚢 Release & Deployment Issues

#### Issue #14: Create Release Documentation & Packaging
**Type:** Documentation/Release | **Priority:** 🟡 MED | **Effort:** 2 days

**Problem:** Need research-grade presentation and release materials.

**Solution:**
- Tagged release (v0.1) with polished repository
- Release process documentation
- Demo materials (GIF, asciinema recordings)
- Academic presentation materials

**Acceptance Criteria:**
- [ ] Tagged release v0.1
- [ ] Release documentation
- [ ] Demo materials created
- [ ] Academic presentation ready

---

#### Issue #15: Add GitHub Actions CI/CD
**Type:** Infrastructure | **Priority:** 🟡 MED | **Effort:** 2-3 days

**Problem:** Need automated testing and continuous integration.

**Solution:**
- Automated testing on multiple platforms
- Performance regression detection
- Automated benchmarking and reporting
- Release automation

**Acceptance Criteria:**
- [ ] Multi-platform testing
- [ ] Performance regression detection
- [ ] Automated benchmarking
- [ ] Release automation

---

#### Issue #16: Enhance Build System
**Type:** Infrastructure | **Priority:** 🟢 LOW | **Effort:** 1-2 days

**Problem:** Build system could be more robust and feature-complete.

**Solution:**
- Add CMake targets for autotuning
- Improve cross-platform compatibility
- Add packaging and installation targets
- Development vs production configurations

**Acceptance Criteria:**
- [ ] CMake target for autotuning
- [ ] Cross-platform compatibility
- [ ] Installation targets
- [ ] Build configuration options

---

#### Issue #17: Add Advanced Optimization Passes
**Type:** Enhancement | **Priority:** 🟢 LOW | **Effort:** 5+ days

**Problem:** Future research potential with additional optimizations.

**Solution:**
- Dead code elimination
- Constant folding and propagation
- Common subexpression elimination
- Register allocation improvements

**Acceptance Criteria:**
- [ ] Additional optimization passes
- [ ] Integration with autotuning
- [ ] Performance analysis
- [ ] Research publication potential

---

## 🗓️ Implementation Timeline

**Week 1 (Critical Infrastructure):**
- Issues #1, #2, #3, #4 (CLI parsing, optimization passes)
- Issue #5 (dependency documentation)

**Week 2 (Autotuning Core):**
- Issues #6, #7, #8 (benchmarks, autotuner, measurement)
- Issue #9 (unit testing)

**Week 3 (Research & Analysis):**
- Issue #10 (performance analysis & tech report)
- Issues #11, #12 (OpenTuner, integration testing)

**Week 4 (Polish & Release):**
- Issues #13, #14, #15 (documentation, release, CI/CD)
- Issue #16 (build system enhancements)

**Future Work:**
- Issue #17 (advanced optimizations)

## 🎯 Success Metrics

By completion, the project should achieve:

1. **Performance:** ≥1.25× speed-up on ≥3/5 benchmarks
2. **Efficiency:** <2× compile-time overhead
3. **Correctness:** 100% test pass rate across all optimization combinations
4. **Research Quality:** Academic-grade technical report and documentation
5. **Usability:** One-line autotuning command for optimization exploration

This comprehensive issue plan transforms Tubular from a "cool toy compiler" into a research-grade autotuning prototype suitable for academic presentation and future research development.