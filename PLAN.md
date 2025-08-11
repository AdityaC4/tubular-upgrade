
Four focused weeks is enough to turn **Tubular** from “cool toy compiler” into a _research-grade_ autotuning prototype that admissions committees can’t ignore.  
The plan below shows you exactly **what to build, when to build it, and why each step signals research potential**. At the end you’ll have (1) measurable speed-ups, (2) a short tech report, and (3) a polished repo link you can drop into every SoP-email thread.

---

## 0 ∙ Groundwork (2 days)

|Task|How|Why it matters|
|---|---|---|
|**Clone & sanity-check** Tubular|`git clone … && make && make tests` – all tests must pass.|Establishes a reproducible baseline ([GitHub](https://github.com/AdityaC4/Tubular "GitHub - AdityaC4/Tubular: Tubular -> WebAssembly Compiler"))|
|**Install tooling**|_Windows_ WSL 2 + Ubuntu 24.04; `sudo apt install wasmtime binaryen python3`. • `wat2wasm` ⇒ turns your WAT into a runnable `.wasm`. • `wasmtime` ⇒ fast runtime + CLI timing flags (`-q --invoke …`). ([Wasmtime](https://docs.wasmtime.dev/cli.html "Using the Wasmtime CLI - Wasmtime"))|Gives deterministic timing on your Ryzen 5 / GTX 1650 without extra infra.|
|**Create a baseline timer**|Tiny Python script: compile → `wat2wasm` → `wasmtime run` (capture `time.perf_counter`). Run each test 30×, keep median.|You’ll need quantifiable numbers to prove autotuning helps.|

---

## 1 ∙ Define the **Tuning Space** (Week 1)

1. **Pick 3 orthogonal knobs** you can generate _at code-gen time_:
    
    |Knob|Choices (initial)|Implementation hint|
    |---|---|---|
    |**Function-inlining**|on / off|Emit direct call vs. inline body.|
    |**Loop unroll factor**|1, 2, 4|Duplicate loop body × factor; guard remainder.|
    |**Tail-recursion style**|as-is / loopify|Detect simple tail-rec & emit `while` loop.|
    
    (All are classical MLIR/LLVM passes too – admissions folks will notice ) ([mlir.llvm.org](https://mlir.llvm.org/docs/Passes/ "Passes - MLIR"))
    
2. **Expose knobs via CLI**  
    `./tubularc --opt inline=1,unroll=4,tail=loop file.tube`
    
3. **Guarantee correctness**  
    Run existing `tests/` after every variant; autotuner only keeps configs that pass.
    

---

## 2 ∙ Build the **Autotuner Loop** (Week 2)

1. **Minimal search engine**
    
    - Start with _grid search_ across ≤24 configs.
        
    - Encode each config as JSON → recompile → time.
        
2. **Plug-in OpenTuner (stretch)**  
    If time allows, call the **OpenTuner** Python API – its ensemble search heuristics often find better configs with <50 runs ([opentuner.org](https://opentuner.org/ "OpenTuner - An extensible framework for program autotuning")).
    
3. **Benchmarks**
    
    - Use five tiny programs: Fib (40), factorial(15), a 1000-iteration `for` loop, string concat, and an array sum.
        
    - Each < 5 ms baseline, avoids GPU need.
        

---

## 3 ∙ Measure & Analyse (Week 3)

|Metric|How to capture|Goal|
|---|---|---|
|**Speed-up vs. baseline**|median(time_baseline / time_tuned)|≥ 1.25× on ≥3/5 benchmarks is compelling.|
|**Compile-time cost**|measure wall-clock during compile only|< 2× baseline shows practicality.|
|**Code-size Δ**|use `wc -c` on `.wasm`|Non-exploding size reassures reviewers.|

For context, WebAssembly runtimes often vary by _orders of magnitude_ on micro-benchmarks – even small IR tweaks matter ([Frank DENIS random thoughts.](https://00f.net/2023/01/04/webassembly-benchmark-2023/ "Performance of WebAssembly runtimes in 2023 - Frank DENIS random thoughts.")).

---

## 4 ∙ Write the **Mini-Paper + Polished Repo** (Week 4)

1. **Tech report (4 pp)**
    
    - _Problem_: Hand-crafted heuristics may underperform across programs/hardware.
        
    - _Method_: Grid/ensemble autotuning over 3 code-gen knobs.
        
    - _Results_: Table + bar chart of speed-ups, compile-time, size.
        
    - _Discussion_: Why more knobs / ML search could push >2×.  
        Cite OpenTuner, ML-based compiler research, and MLIR loop-fusion literature. ([arXiv](https://arxiv.org/abs/2201.13305?utm_source=chatgpt.com "Optimizing LLVM Pass Sequences with Shackleton"), [LLVM](https://llvm.org/devmtg/2021-02-28/slides/Denis-perf.pdf?utm_source=chatgpt.com "Performance Tuning: Future Compiler Improvements"), [Xilinx GitHub Pages](https://xilinx.github.io/mlir-air/AIRTransformPasses.html?utm_source=chatgpt.com "AMD AIR MLIR Dialect"))
        
2. **Repo cleanup**
    
    - Branch `autotune-mvp`, tag `v0.1`.
        
    - README section _“--tune in one line”_ with GIF or asciinema demo.
        
    - GitHub Actions to run benches on every push (optional).
        
3. **Email-ready one-liner**
    
    > “I built Tubular, a toy language → WebAssembly compiler, then added a search-based optimizer that picks faster code-gen strategies and delivers up to 1.4× speed-ups over baseline on Wasmtime.”
    

That sentence lands with **Adve, Pingali, Sarkar, Hall, Ahmed** because it speaks their language of _performance-portable, search-driven compilation_.

---

## Timeline Snapshot

|Date (2025)|Milestone|
|---|---|
|**Aug 10 – 12**|Toolchain ready; baseline times logged.|
|**Aug 13 – 19**|Knobs implemented & validated.|
|**Aug 20 – 27**|Autotuner loop + benchmark suite; first speed-ups.|
|**Aug 28 – Sep 3**|Data collection, plots, tech report draft.|
|**Sep 4 – 6**|Final polish, repo tag `v0.1`, send first advisor emails.|

---

## Why Admissions Committees Will Care

_Autotuning_ turns an engineering demo into a **research question**: _“How do different code-generation choices interact with runtime performance on real hardware, and can we automate that choice?”_  
This resonates with current PL/compiler trends toward **ML-guided optimization** and **search-based IR transformation** ([LLVM](https://llvm.org/devmtg/2021-02-28/slides/Denis-perf.pdf?utm_source=chatgpt.com "Performance Tuning: Future Compiler Improvements"), [mlir.llvm.org](https://mlir.llvm.org/docs/Dialects/Transform/?utm_source=chatgpt.com "Transform Dialect - MLIR - LLVM")) while staying small enough to finish in a month.

Nail this roadmap, and your application will showcase:

1. **Compiler construction chops** (front-end → WAT backend).
    
2. **Quantitative research thinking** (hypothesis, experiment, result).
    
3. **Awareness of state-of-the-art autotuning literature**.
    

That’s precisely the trio that gets faculty to say _“let’s interview this person.”
