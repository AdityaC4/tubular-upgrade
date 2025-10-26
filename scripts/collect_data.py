#!/usr/bin/env python3
"""
End-to-end data collection helper for the Tubular compiler project.

This script rebuilds the compiler, runs the full regression test suite,
executes the autotuning sweep (including all pass-order permutations),
and distills the resulting CSV into a JSON summary that is ready for
inclusion in reports.
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


@dataclass
class CommandResult:
    cmd: List[str]
    returncode: int
    stdout: str
    stderr: str


def run_command(cmd: List[str], *, cwd: Path | None = None) -> CommandResult:
    print(f"[collect] running: {' '.join(cmd)}")
    completed = subprocess.run(
        cmd,
        cwd=cwd,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        print(f"[collect] command failed with exit code {completed.returncode}", file=sys.stderr)
        raise SystemExit(completed.returncode)
    return CommandResult(cmd, completed.returncode, "", "")


def parse_results(csv_path: Path) -> Tuple[Dict[Tuple[str, str], List[Dict]], Dict[str, List[Dict]]]:
    rows_by_variant: Dict[Tuple[str, str], List[Dict]] = defaultdict(list)
    rows_by_pass: Dict[str, List[Dict]] = defaultdict(list)
    with csv_path.open(newline="", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            key = (row["benchmark"], row["variant"])
            row["median_ms"] = float(row["median_ms"])
            rows_by_variant[key].append(row)
            rows_by_pass[row["pass_order"]].append(row)
    if not rows_by_variant:
        raise RuntimeError(f"No rows found in {csv_path}")
    return rows_by_variant, rows_by_pass


def summarize_variants(rows_by_variant: Dict[Tuple[str, str], List[Dict]]) -> List[Dict]:
    summary = []
    for (bench, variant), entries in sorted(rows_by_variant.items()):
        best = min(entries, key=lambda r: r["median_ms"])
        worst = max(entries, key=lambda r: r["median_ms"])
        gap = worst["median_ms"] - best["median_ms"]
        pct = (gap / worst["median_ms"] * 100) if worst["median_ms"] else 0.0
        summary.append(
            {
                "benchmark": bench,
                "variant": variant,
                "best_order": best["pass_order"],
                "best_flags": best["flags"],
                "best_median_ms": best["median_ms"],
                "worst_order": worst["pass_order"],
                "worst_flags": worst["flags"],
                "worst_median_ms": worst["median_ms"],
                "delta_ms": gap,
                "delta_pct": pct,
            }
        )
    return summary


def summarize_pass_orders(rows_by_pass: Dict[str, List[Dict]]) -> List[Dict]:
    summary = []
    for order, entries in sorted(rows_by_pass.items()):
        medians = [row["median_ms"] for row in entries]
        avg = sum(medians) / len(medians)
        summary.append(
            {
                "pass_order": order,
                "num_samples": len(entries),
                "mean_median_ms": avg,
                "min_median_ms": min(medians),
                "max_median_ms": max(medians),
            }
        )
    return summary


def write_summary(summary_path: Path, *, config: Path, variant_stats: List[Dict], pass_stats: List[Dict]) -> None:
    summary = {
        "generated_at": datetime.utcnow().isoformat(timespec="seconds") + "Z",
        "config": str(config),
        "variant_stats": variant_stats,
        "pass_order_stats": pass_stats,
    }
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    with summary_path.open("w", encoding="utf-8") as fh:
        json.dump(summary, fh, indent=2)
    print(f"[collect] wrote summary to {summary_path}")


def print_human_summary(variant_stats: Iterable[Dict], pass_stats: Iterable[Dict], top_n: int = 10) -> None:
    print("\n=== Top variant deltas (best vs worst pass order) ===")
    ranked = sorted(variant_stats, key=lambda s: s["delta_pct"], reverse=True)
    for entry in ranked[:top_n]:
        print(
            f"{entry['benchmark']}/{entry['variant']}: "
            f"{entry['best_order']} ({entry['best_median_ms']:.3f} ms) vs "
            f"{entry['worst_order']} ({entry['worst_median_ms']:.3f} ms) "
            f"→ Δ {entry['delta_ms']:.3f} ms ({entry['delta_pct']:.1f} %)"
        )

    print("\n=== Pass order aggregates (lower is better) ===")
    for entry in sorted(pass_stats, key=lambda s: s["mean_median_ms"]):
        print(
            f"{entry['pass_order']}: mean={entry['mean_median_ms']:.3f} ms "
            f"(min={entry['min_median_ms']:.3f}, max={entry['max_median_ms']:.3f}, "
            f"samples={entry['num_samples']})"
        )


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Tubular data collection pipeline")
    parser.add_argument("--project-root", type=Path, default=Path("."), help="Repository root (default: .)")
    parser.add_argument(
        "--config",
        type=Path,
        default=Path("research_tests/config.json"),
        help="Autotuning config (default: research_tests/config.json)",
    )
    parser.add_argument(
        "--results",
        type=Path,
        default=Path("artifacts/research/results.csv"),
        help="Autotuning results CSV (default: artifacts/research/results.csv)",
    )
    parser.add_argument(
        "--summary",
        type=Path,
        default=Path("artifacts/research/summary.json"),
        help="Summary JSON output path (default: artifacts/research/summary.json)",
    )
    parser.add_argument("--skip-build", action="store_true", help="Skip ./make")
    parser.add_argument("--skip-tests", action="store_true", help="Skip ./make test")
    parser.add_argument("--skip-autotune", action="store_true", help="Skip autotuning run")
    parser.add_argument(
        "--autotune-out-dir",
        type=Path,
        default=Path("artifacts/research/out"),
        help="Autotuning artifact dir (default: artifacts/research/out)",
    )
    args = parser.parse_args(argv)

    root = args.project_root.resolve()
    make_script = root / "make"
    autotune_script = root / "autotuning" / "run_autotune.py"

    if not args.skip_build:
        run_command(["./make"], cwd=root)
    if not args.skip_tests:
        run_command(["./make", "test"], cwd=root)
    if not args.skip_autotune:
        run_command(
            [
                sys.executable,
                str(autotune_script),
                "--config",
                str(args.config),
                "--output",
                str(args.results),
                "--out-dir",
                str(args.autotune_out_dir),
            ],
            cwd=root,
        )

    rows_by_variant, rows_by_pass = parse_results(root / args.results)
    variant_stats = summarize_variants(rows_by_variant)
    pass_stats = summarize_pass_orders(rows_by_pass)
    write_summary(root / args.summary, config=args.config, variant_stats=variant_stats, pass_stats=pass_stats)
    print_human_summary(variant_stats, pass_stats)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
