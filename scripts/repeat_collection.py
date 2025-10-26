#!/usr/bin/env python3
"""
Helper to run scripts/collect_data.py multiple times in succession and store each
run's outputs in its own directory (results, summary, logs, intermediates).
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import List


def run_command(cmd: List[str], *, cwd: Path, log_file: Path) -> None:
    log_file.parent.mkdir(parents=True, exist_ok=True)
    with log_file.open("w", encoding="utf-8") as log:
        log.write(f"$ {' '.join(cmd)}\n\n")
        process = subprocess.Popen(
            cmd,
            cwd=cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        for line in process.stdout:
            log.write(line)
            print(line, end="")
        ret = process.wait()
        log.write(f"\n[exit {ret}]\n")
        if ret != 0:
            raise SystemExit(f"Command failed (see {log_file})")


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Repeat collect_data runs with isolated artifacts")
    parser.add_argument("--runs", type=int, default=3, help="How many times to run the pipeline (default: 3)")
    parser.add_argument("--project-root", type=Path, default=Path("."), help="Repository root")
    parser.add_argument(
        "--collector",
        type=Path,
        default=Path("scripts/collect_data.py"),
        help="Path to collect_data.py (default: scripts/collect_data.py)",
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=Path("research_tests/config.json"),
        help="Autotuning config (default: research_tests/config.json)",
    )
    parser.add_argument(
        "--results-root",
        type=Path,
        default=Path("artifacts/research/batch_runs"),
        help="Directory to store per-run artifacts",
    )
    parser.add_argument(
        "--skip-tests-after-first",
        action="store_true",
        help="Skip ./make test after the first run to save time",
    )
    parser.add_argument(
        "--skip-build-after-first",
        action="store_true",
        help="Skip ./make after the first run to save time",
    )
    parser.add_argument(
        "--extra-collector-args",
        nargs=argparse.REMAINDER,
        help="Additional arguments passed through to collect_data.py (add after --)",
    )
    args = parser.parse_args(argv)

    root = args.project_root.resolve()
    collector = (root / args.collector).resolve()
    if not collector.exists():
        raise SystemExit(f"Collector not found: {collector}")

    results_root = (root / args.results_root).resolve()
    results_root.mkdir(parents=True, exist_ok=True)

    metadata = []
    for idx in range(1, args.runs + 1):
        run_dir = results_root / f"run{idx}"
        run_dir.mkdir(parents=True, exist_ok=True)
        summary_path = run_dir / "summary.json"
        results_path = run_dir / "results.csv"
        out_dir = run_dir / "out"
        log_file = run_dir / "collect.log"

        cmd = [
            sys.executable,
            str(collector),
            "--project-root",
            str(root),
            "--config",
            str(args.config),
            "--results",
            str(results_path),
            "--summary",
            str(summary_path),
            "--autotune-out-dir",
            str(out_dir),
        ]

        if args.skip_tests_after_first and idx > 1:
            cmd.append("--skip-tests")
        if args.skip_build_after_first and idx > 1:
            cmd.append("--skip-build")
        if args.extra_collector_args:
            cmd.extend(args.extra_collector_args)

        print(f"\n=== Run {idx}/{args.runs} ===")
        run_command(cmd, cwd=root, log_file=log_file)

        # Parse summary for a quick digest
        summary_data = json.load(summary_path.open())
        top_entry = max(
            summary_data["variant_stats"],
            key=lambda row: row.get("delta_pct", 0.0),
        )
        metadata.append(
            {
                "run": idx,
                "summary": str(summary_path.relative_to(root)),
                "results": str(results_path.relative_to(root)),
                "log": str(log_file.relative_to(root)),
                "timestamp": datetime.utcnow().isoformat(timespec="seconds") + "Z",
                "top_delta": {
                    "benchmark": top_entry["benchmark"],
                    "variant": top_entry["variant"],
                    "delta_pct": top_entry["delta_pct"],
                    "best_order": top_entry["best_order"],
                    "worst_order": top_entry["worst_order"],
                },
            }
        )

    manifest_path = results_root / "manifest.json"
    with manifest_path.open("w", encoding="utf-8") as fh:
        json.dump({"runs": metadata}, fh, indent=2)
    print(f"\nWrote manifest to {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
