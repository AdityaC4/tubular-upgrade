#!/usr/bin/env python3
"""
Utility to aggregate multiple research data runs, rank deltas, and emit plots.

It expects summary JSON files (as emitted by scripts/collect_data.py) either in
artifacts/research/batch_runs/run*/summary.json or provided via arguments.
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Tuple


def load_summaries(paths: List[Path]) -> List[Dict]:
    summaries = []
    for path in paths:
        if not path.exists():
            raise SystemExit(f"Summary not found: {path}")
        with path.open() as fh:
            summaries.append(json.load(fh))
    return summaries


def find_default_summaries(project_root: Path) -> List[Path]:
    batch_dir = project_root / "artifacts" / "research" / "batch_runs"
    if batch_dir.exists():
        summaries = sorted(batch_dir.glob("run*/summary.json"))
        if summaries:
            return summaries
    # fallback to latest single summary
    return [project_root / "artifacts" / "research" / "summary.json"]


def aggregate_variant_deltas(summaries: List[Dict]) -> List[Tuple[str, str, float]]:
    deltas = defaultdict(list)
    for summary in summaries:
        for row in summary["variant_stats"]:
            key = (row["benchmark"], row["variant"])
            deltas[key].append(row["delta_pct"])
    averaged = [(bench, variant, sum(values) / len(values)) for (bench, variant), values in deltas.items()]
    averaged.sort(key=lambda x: x[2], reverse=True)
    return averaged


def aggregate_pass_order_means(summaries: List[Dict]) -> Dict[str, float]:
    totals = defaultdict(list)
    for summary in summaries:
        for row in summary["pass_order_stats"]:
            totals[row["pass_order"]].append(row["mean_median_ms"])
    return {order: sum(values) / len(values) for order, values in totals.items()}


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate plots/tables from research summary files")
    parser.add_argument("--project-root", type=Path, default=Path("."), help="Repository root (default: .)")
    parser.add_argument("--summaries", type=Path, nargs="*", help="Explicit summary.json paths to aggregate")
    parser.add_argument("--out-dir", type=Path, default=Path("docs/figures"), help="Directory for output tables")
    parser.add_argument("--top-n", type=int, default=10, help="How many top deltas to include")
    args = parser.parse_args()

    root = args.project_root.resolve()
    if args.summaries:
        summary_paths = [path if path.is_absolute() else root / path for path in args.summaries]
    else:
        summary_paths = find_default_summaries(root)
    summaries = load_summaries(summary_paths)

    top_stats = aggregate_variant_deltas(summaries)
    pass_means = aggregate_pass_order_means(summaries)

    out_dir = (root / args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    top_table = out_dir / "top_deltas_table.csv"
    with top_table.open("w") as fh:
        fh.write("benchmark,variant,label,avg_delta_pct\n")
        for bench, variant, delta in top_stats[: args.top_n]:
            label = f"{bench} ({variant})"
            fh.write(f"{bench},{variant},{label},{delta:.4f}\n")

    pass_table = out_dir / "pass_order_means.csv"
    with pass_table.open("w") as fh:
        fh.write("pass_order,mean_median_ms\n")
        for order, value in sorted(pass_means.items()):
            fh.write(f"{order},{value:.4f}\n")

    # dump aggregated data for LaTeX/analysis
    aggregated = {
        "summaries": [str(path.relative_to(root)) for path in summary_paths],
        "top_variant_stats": top_stats,
        "pass_order_means": pass_means,
    }
    agg_path = root / "artifacts" / "research" / "aggregated_metrics.json"
    agg_path.parent.mkdir(parents=True, exist_ok=True)
    with agg_path.open("w") as fh:
        json.dump(aggregated, fh, indent=2)
    print(f"Wrote tables to {args.out_dir} and metrics to {agg_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
