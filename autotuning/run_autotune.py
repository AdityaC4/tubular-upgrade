#!/usr/bin/env python3
"""
Autotuning runner for the Tubular compiler.

Compiles benchmarks with a set of optimization flag variants, executes each
generated WebAssembly module via Node.js, and writes timing results to CSV.
"""

from __future__ import annotations

import argparse
import csv
import json
import shutil
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

NODE_RUNNER = (
    "const fs=require('fs');"
    "(async()=>{"
    "try{"
    "const wasmPath=process.argv[1];"
    "const fn=process.argv[2]||'main';"
    "const bytes=fs.readFileSync(wasmPath);"
    "const {instance}=await WebAssembly.instantiate(bytes);"
    "const result=instance.exports[fn]();"
    "process.stdout.write(String(result));"
    "}catch(err){console.error(err);process.exit(1);}"
    "})();"
)

PASS_ORDER_TOKENS = {"inline", "unroll", "tail"}


def shell_available(executable: str) -> bool:
    return shutil.which(executable) is not None


def run_command(cmd: List[str], *, cwd: Path | None = None,
                stdout=None, stderr=subprocess.PIPE) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd,
        cwd=cwd,
        stdout=stdout,
        stderr=stderr,
        text=True,
        check=True,
    )


def ensure_tools(tubular: Path, wat2wasm: str, node_exec: str) -> None:
    missing: List[str] = []
    if not tubular.exists():
        missing.append(f"Tubular executable not found at {tubular}")
    if not shell_available(wat2wasm):
        missing.append("'wat2wasm' not found in PATH")
    if not shell_available(node_exec):
        missing.append("'node' not found in PATH")
    if missing:
        raise SystemExit("Missing requirements:\n  " + "\n  ".join(missing))


def load_config(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def compile_benchmark(tubular: Path, bench_path: Path, flags: List[str],
                      wat_path: Path) -> None:
    wat_path.parent.mkdir(parents=True, exist_ok=True)
    with wat_path.open("w", encoding="utf-8") as out:
        run_command([str(tubular), str(bench_path), *flags], stdout=out)


def convert_wasm(wat2wasm: str, wat_path: Path, wasm_path: Path) -> None:
    wasm_path.parent.mkdir(parents=True, exist_ok=True)
    run_command([wat2wasm, str(wat_path), "-o", str(wasm_path)], stdout=subprocess.PIPE)


def run_wasm(node_exec: str, wasm_path: Path, invoke: str) -> subprocess.CompletedProcess:
    return run_command([node_exec, "-e", NODE_RUNNER, str(wasm_path), invoke],
                       stdout=subprocess.PIPE)


def normalize_pass_token(token: str) -> str:
    trimmed = token.strip().lower()
    if trimmed not in PASS_ORDER_TOKENS:
        raise ValueError(f"Invalid pass name '{token}' (expected inline, unroll, tail)")
    return trimmed


def normalize_pass_order_entry(entry: Dict[str, Any]) -> Dict[str, Any]:
    name = entry.get("name")
    order = entry.get("order", [])
    if not name:
        raise ValueError("Each pass_orders entry must have a 'name'")
    if len(order) != len(PASS_ORDER_TOKENS):
        raise ValueError(
            f"Pass order '{name}' must specify {len(PASS_ORDER_TOKENS)} passes (got {len(order)})"
        )

    normalized = [normalize_pass_token(token) for token in order]
    if len(set(normalized)) != len(PASS_ORDER_TOKENS):
        raise ValueError(f"Pass order '{name}' must list each pass exactly once")
    return {"name": name, "order": normalized}


def load_pass_orders(config: Dict[str, Any]) -> List[Dict[str, Any]]:
    entries = config.get("pass_orders")
    if not entries:
        return [{"name": "inline-unroll-tail", "order": ["inline", "unroll", "tail"]}]

    normalized_entries: List[Dict[str, Any]] = []
    for entry in entries:
        normalized_entries.append(normalize_pass_order_entry(entry))
    return normalized_entries


def measure_variant(
    tubular: Path,
    wat2wasm: str,
    node_exec: str,
    bench: Dict[str, Any],
    variant_name: str,
    flags: List[str],
    pass_order_name: str,
    output_dir: Path,
    runs: int,
    warmup_runs: int,
) -> Dict[str, Any]:
    bench_name = bench["name"]
    benchmark_path = Path(bench["path"])
    if not benchmark_path.exists():
        raise FileNotFoundError(f"Benchmark file not found: {benchmark_path}")

    wat_suffix = pass_order_name.replace(" ", "_")
    wat_path = output_dir / f"{bench_name}__{variant_name}__{wat_suffix}.wat"
    wasm_path = output_dir / f"{bench_name}__{variant_name}__{wat_suffix}.wasm"

    compile_benchmark(tubular, benchmark_path, flags, wat_path)
    convert_wasm(wat2wasm, wat_path, wasm_path)

    invoke = bench.get("invoke", "main")
    expected = bench.get("expected")

    # Warm-up: execute but discard timings.
    for _ in range(max(0, warmup_runs)):
        run_wasm(node_exec, wasm_path, invoke)

    timings: List[float] = []
    results: List[str] = []
    for _ in range(runs):
        start = time.perf_counter()
        completed = run_wasm(node_exec, wasm_path, invoke)
        elapsed_ms = (time.perf_counter() - start) * 1000.0
        timings.append(elapsed_ms)
        results.append(completed.stdout.strip())

    if not results:
        raise RuntimeError("No timing data recorded.")

    canonical = results[0]
    if not all(r == canonical for r in results):
        raise RuntimeError(f"Inconsistent outputs for {bench_name}/{variant_name}: {results}")
    if expected is not None and canonical != str(expected):
        raise RuntimeError(
            f"Output mismatch for {bench_name}/{variant_name}: expected {expected}, got {canonical}"
        )

    timings.sort()
    median = statistics.median(timings)
    p25 = statistics.quantiles(timings, n=4)[0] if len(timings) >= 4 else timings[0]
    p75 = statistics.quantiles(timings, n=4)[-1] if len(timings) >= 4 else timings[-1]

    return {
        "benchmark": bench_name,
        "variant": variant_name,
        "pass_order": pass_order_name,
        "flags": " ".join(flags),
        "wat_size": wat_path.stat().st_size,
        "wasm_size": wasm_path.stat().st_size,
        "runs": runs,
        "warmup_runs": warmup_runs,
        "p25_ms": p25,
        "median_ms": median,
        "p75_ms": p75,
        "result": canonical,
    }


def write_csv(rows: List[Dict[str, Any]], output_path: Path) -> None:
    if not rows:
        return
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "benchmark",
        "variant",
        "pass_order",
        "flags",
        "wat_size",
        "wasm_size",
        "runs",
        "warmup_runs",
        "p25_ms",
        "median_ms",
        "p75_ms",
        "result",
    ]
    with output_path.open("w", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Tubular autotuning runner")
    parser.add_argument(
        "--config",
        type=Path,
        default=Path("autotuning/config/default.json"),
        help="Path to autotuning configuration JSON (default: %(default)s)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("autotuning/results.csv"),
        help="Path to write CSV results (default: %(default)s)",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("autotuning/out"),
        help="Directory for intermediate WAT/WASM artifacts (default: %(default)s)",
    )
    parser.add_argument(
        "--tubular",
        type=Path,
        default=Path("build/Tubular"),
        help="Path to the Tubular compiler executable (default: %(default)s)",
    )
    parser.add_argument(
        "--wat2wasm",
        default="wat2wasm",
        help="wat2wasm executable (default: %(default)s)",
    )
    parser.add_argument(
        "--node",
        default="node",
        help="Node.js executable (default: %(default)s)",
    )
    parser.add_argument(
        "--runs",
        type=int,
        default=None,
        help="Override number of timed runs per variant",
    )
    parser.add_argument(
        "--warmup",
        type=int,
        default=None,
        help="Override number of warm-up runs per variant",
    )

    args = parser.parse_args(argv)

    config = load_config(args.config)
    runs = args.runs if args.runs is not None else config.get("runs", 5)
    warmup = args.warmup if args.warmup is not None else config.get("warmup_runs", 1)

    ensure_tools(args.tubular, args.wat2wasm, args.node)

    benchmarks = config.get("benchmarks", [])
    variants = config.get("variants", [])
    try:
        pass_orders = load_pass_orders(config)
    except ValueError as exc:
        raise SystemExit(f"Invalid pass order configuration: {exc}") from exc
    if not benchmarks or not variants:
        raise SystemExit("Configuration must include non-empty 'benchmarks' and 'variants'.")

    results: List[Dict[str, Any]] = []
    for bench in benchmarks:
        for variant in variants:
            base_flags = list(variant.get("flags", []))
            for order in pass_orders:
                order_flags = base_flags.copy()
                order_name = order["name"]
                if order["order"]:
                    order_flags.append(f"--pass-order={','.join(order['order'])}")
                print(
                    f"[RUN] {bench['name']} / {variant['name']} [{order_name}]",
                    flush=True,
                )
                try:
                    result = measure_variant(
                        args.tubular,
                        args.wat2wasm,
                        args.node,
                        bench,
                        variant["name"],
                        order_flags,
                        order_name,
                        args.out_dir,
                        runs,
                        warmup,
                    )
                    results.append(result)
                    print(
                        f"[OK] {bench['name']} / {variant['name']} [{order_name}]: "
                        f"{result['median_ms']:.3f} ms (flags: {result['flags']})"
                    )
                except subprocess.CalledProcessError as exc:
                    print(
                        f"[ERR] {bench['name']} / {variant['name']} [{order_name}]: command failed\n{exc.stderr}",
                        file=sys.stderr,
                    )
                except Exception as exc:
                    print(
                        f"[ERR] {bench['name']} / {variant['name']} [{order_name}]: {exc}",
                        file=sys.stderr,
                    )

    write_csv(results, args.output)
    print(f"\nWrote {len(results)} rows to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
