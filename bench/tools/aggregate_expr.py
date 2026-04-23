#!/usr/bin/env python3
import argparse
import csv
import glob
import math
import os
from collections import Counter, defaultdict


NUMERIC_FIELDS = [
    "prepare_ms",
    "cold_run_ms",
    "e2e_n1_ms",
    "e2e_n10_ms",
    "e2e_n100_ms",
    "trimmed_mean_ms",
    "cv_percent",
    "ci95_low_ms",
    "ci95_high_ms",
    "ci95_half_ms",
    "gelem_per_s",
    "gflops",
    "max_abs_err",
    "max_rel_err",
]


def fnum(row, key, default=0.0):
    try:
        return float(row.get(key, default))
    except Exception:
        return default


def mean(vals):
    return sum(vals) / len(vals) if vals else 0.0


def stddev(vals):
    if len(vals) < 2:
        return 0.0
    m = mean(vals)
    var = sum((v - m) ** 2 for v in vals) / (len(vals) - 1)
    return math.sqrt(var)


def ci95_half(vals):
    if len(vals) < 2:
        return 0.0
    return 1.96 * stddev(vals) / math.sqrt(len(vals))


def parse_args():
    ap = argparse.ArgumentParser(description="Aggregate expression benchmark CSV files")
    ap.add_argument("--input-glob", required=True, help="Input CSV glob pattern")
    ap.add_argument("--output-dir", required=True, help="Output directory")
    ap.add_argument("--platform", default="unknown", help="Platform label in report")
    return ap.parse_args()


def main():
    args = parse_args()
    files = sorted(glob.glob(args.input_glob))
    if not files:
        raise SystemExit(f"no files matched: {args.input_glob}")

    rows = []
    for fp in files:
        with open(fp, "r", encoding="utf-8-sig", newline="") as f:
            r = csv.DictReader(f)
            for row in r:
                row["source_file"] = os.path.basename(fp)
                rows.append(row)

    by_group = defaultdict(list)
    for row in rows:
        key = (
            row.get("case_id", ""),
            row.get("input_profile", ""),
            row.get("type", ""),
            row.get("threads", ""),
            row.get("method", ""),
        )
        by_group[key].append(row)

    summary = []
    for key, grp in by_group.items():
        case_id, input_profile, vtype, threads, method = key
        available_vals = [1 if x.get("available", "0") == "1" else 0 for x in grp]
        correct_vals = [1 if x.get("correct", "0") == "1" else 0 for x in grp]
        reasons = [x.get("reason", "") for x in grp if x.get("reason", "")]

        item = {
            "platform": args.platform,
            "case_id": case_id,
            "input_profile": input_profile,
            "type": vtype,
            "threads": threads,
            "method": method,
            "samples": len(grp),
            "available_rate": mean(available_vals),
            "correct_rate": mean(correct_vals),
            "reason_mode": Counter(reasons).most_common(1)[0][0] if reasons else "",
        }

        for field in NUMERIC_FIELDS:
            vals = [fnum(x, field) for x in grp if x.get("available", "0") == "1" and x.get("correct", "0") == "1"]
            item[f"{field}_mean"] = mean(vals) if vals else 0.0
            item[f"{field}_std"] = stddev(vals) if vals else 0.0
            item[f"{field}_ci95_half"] = ci95_half(vals) if vals else 0.0
        summary.append(item)

    # speedup vs hardcoded_plain_loop4 by (case,type,threads)
    baseline = {}
    for s in summary:
        if s["method"] == "hardcoded_plain_loop4":
            k = (s["case_id"], s["type"], s["threads"])
            baseline[k] = s["trimmed_mean_ms_mean"]
    for s in summary:
        k = (s["case_id"], s["type"], s["threads"])
        b = baseline.get(k, 0.0)
        t = s["trimmed_mean_ms_mean"]
        s["speedup_vs_plain_loop4"] = (b / t) if (b > 0 and t > 0) else 0.0

    os.makedirs(args.output_dir, exist_ok=True)
    merged_path = os.path.join(args.output_dir, "expr_matrix_raw_merged.csv")
    with open(merged_path, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    summary_fields = [
        "platform",
        "case_id",
        "input_profile",
        "type",
        "threads",
        "method",
        "samples",
        "available_rate",
        "correct_rate",
        "speedup_vs_plain_loop4",
        "reason_mode",
    ]
    for field in NUMERIC_FIELDS:
        summary_fields.extend([f"{field}_mean", f"{field}_std", f"{field}_ci95_half"])

    summary_path = os.path.join(args.output_dir, "expr_matrix_summary.csv")
    with open(summary_path, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=summary_fields)
        w.writeheader()
        w.writerows(sorted(summary, key=lambda x: (x["case_id"], x["type"], int(x["threads"] or "0"), x["trimmed_mean_ms_mean"])))

    # markdown report
    md_path = os.path.join(args.output_dir, "expr_matrix_summary.md")
    grouped = defaultdict(list)
    for s in summary:
        grouped[(s["case_id"], s["type"], s["threads"])].append(s)

    with open(md_path, "w", encoding="utf-8") as f:
        f.write(f"# Expr Benchmark Matrix Summary ({args.platform})\n\n")
        f.write(f"- input files: {len(files)}\n")
        f.write(f"- raw rows: {len(rows)}\n\n")
        for (case_id, vtype, threads), items in sorted(grouped.items()):
            items_sorted = sorted(items, key=lambda x: x["trimmed_mean_ms_mean"] if x["trimmed_mean_ms_mean"] > 0 else 1e100)
            f.write(f"## {case_id} | type={vtype} | threads={threads}\n\n")
            f.write("| method | samples | avail | ok | trimmed_mean_ms | ci95_half_ms | gflops | speedup_vs_plain |\n")
            f.write("|---|---:|---:|---:|---:|---:|---:|---:|\n")
            for s in items_sorted:
                f.write(
                    f"| {s['method']} | {s['samples']} | {s['available_rate']:.2f} | {s['correct_rate']:.2f} | "
                    f"{s['trimmed_mean_ms_mean']:.6f} | {s['trimmed_mean_ms_ci95_half']:.6f} | "
                    f"{s['gflops_mean']:.3f} | {s['speedup_vs_plain_loop4']:.3f}x |\n"
                )
            f.write("\n")

    print(f"wrote: {merged_path}")
    print(f"wrote: {summary_path}")
    print(f"wrote: {md_path}")


if __name__ == "__main__":
    main()

