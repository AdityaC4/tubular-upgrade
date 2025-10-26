#!/usr/bin/env python3
"""
Generate a benchmark feature table (loops, tail recursion, branching) and
identify the dominant pass ordering per benchmark based on aggregated data.
"""

from __future__ import annotations

import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Tuple


FEATURES: Dict[str, Dict[str, str]] = {
    "rt01-fib-recursive": {"loops": "0", "tail": "No (non-tail recursion)", "branch": "High (if)"},
    "rt02-tail-factorial": {"loops": "0", "tail": "Yes (tail recursion)", "branch": "Low"},
    "rt03-loop-summation": {"loops": "1", "tail": "No", "branch": "Low"},
    "rt04-stride-heavy-loop": {"loops": "1", "tail": "No", "branch": "Low"},
    "rt05-nested-mix": {"loops": "2 (nested)", "tail": "No", "branch": "Low"},
    "rt06-string-wrapping": {"loops": "1", "tail": "No", "branch": "Low"},
    "rt07-helper-inline": {"loops": "1", "tail": "No", "branch": "Low"},
    "rt08-branchy-loop": {"loops": "1", "tail": "No", "branch": "High (if/continue)"},
    "rt09-matrix-mix": {"loops": "2 (nested)", "tail": "No", "branch": "Low"},
    "rt10-control-baseline": {"loops": "1", "tail": "No", "branch": "Low"},
}


def load_summaries(run_dirs: List[Path]) -> List[Dict]:
    summaries = []
    for run_dir in run_dirs:
        summary_path = run_dir / "summary.json"
        if summary_path.exists():
            summaries.append(json.load(summary_path.open()))
    if not summaries:
        raise SystemExit("No summary files found for benchmark feature table.")
    return summaries


def best_order_per_variant(summaries: List[Dict]) -> Dict[Tuple[str, str], str]:
    # For each (benchmark, variant) gather rows from each summary
    by_pair: Dict[Tuple[str, str], List[Dict]] = defaultdict(list)
    for summary in summaries:
        for row in summary["variant_stats"]:
            key = (row["benchmark"], row["variant"])
            by_pair[key].append(row)

    majority_order: Dict[Tuple[str, str], str] = {}
    for key, rows in by_pair.items():
        votes = Counter(row["best_order"] for row in rows)
        majority = max(votes.values())
        candidates = [order for order, count in votes.items() if count == majority]
        if len(candidates) == 1:
            majority_order[key] = candidates[0]
            continue
        # Tie-breaker: lowest average best_median_ms (fallback to worst median if needed)
        best_order = None
        best_value = None
        for order in candidates:
            medians = []
            for row in rows:
                if row["best_order"] == order:
                    medians.append(row["best_median_ms"])
                else:
                    medians.append(row["worst_median_ms"])
            avg = sum(medians) / len(medians)
            if best_value is None or avg < best_value:
                best_value = avg
                best_order = order
        majority_order[key] = best_order
    return majority_order


def load_avg_deltas(aggregated_path: Path) -> Dict[Tuple[str, str], float]:
    data = json.load(aggregated_path.open())
    deltas = {}
    for bench, variant, delta in data["top_variant_stats"]:
        deltas[(bench, variant)] = delta
    return deltas


def build_table(
    majority_order: Dict[Tuple[str, str], str],
    avg_deltas: Dict[Tuple[str, str], float],
) -> List[Dict[str, str]]:
    per_benchmark: Dict[str, Counter] = defaultdict(Counter)
    for (bench, variant), order in majority_order.items():
        weight = avg_deltas.get((bench, variant), 0.0)
        per_benchmark[bench][order] += weight

    table_rows: List[Dict[str, str]] = []
    for bench in sorted(FEATURES.keys()):
        order_weights = per_benchmark.get(bench, Counter())
        if order_weights:
            dominant_order, _ = order_weights.most_common(1)[0]
        else:
            dominant_order = "n/a"
        table_rows.append(
            {
                "benchmark": bench,
                "loops": FEATURES[bench]["loops"],
                "tail": FEATURES[bench]["tail"],
                "branch": FEATURES[bench]["branch"],
                "dominant_order": dominant_order.replace("-", "→"),
            }
        )
    return table_rows


def write_csv(rows: List[Dict[str, str]], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as fh:
        fh.write("benchmark,loops,tail_recursion,branching,dominant_order\n")
        for row in rows:
            fh.write(
                f"{row['benchmark']},{row['loops']},{row['tail']},{row['branch']},{row['dominant_order']}\n"
            )


def write_latex(rows: List[Dict[str, str]], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as fh:
        fh.write("\\begin{tabular}{lllll}\n")
        fh.write("\\toprule\n")
        fh.write("Benchmark & Loops & Tail recursion & Branching & Dominant order \\\\\n")
        fh.write("\\midrule\n")
        for row in rows:
            fh.write(
                f"{row['benchmark']} & {row['loops']} & {row['tail']} & {row['branch']} & {row['dominant_order']} \\\\\n"
            )
        fh.write("\\bottomrule\n\\end{tabular}\n")


def main() -> int:
    root = Path(".").resolve()
    batch_runs = sorted((root / "artifacts" / "research" / "batch_runs").glob("run*"))
    if not batch_runs:
        raise SystemExit("No batch runs found under artifacts/research/batch_runs/")
    summaries = load_summaries(batch_runs)
    majority = best_order_per_variant(summaries)
    aggregated_path = root / "artifacts" / "research" / "aggregated_metrics.json"
    avg_deltas = load_avg_deltas(aggregated_path)
    table_rows = build_table(majority, avg_deltas)

    write_csv(table_rows, root / "docs" / "figures" / "benchmark_features_table.csv")
    write_latex(table_rows, root / "docs" / "benchmark_features_table.tex")
    print("Wrote benchmark feature tables.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
