#!/usr/bin/env python3
"""Aggregate run_bench.sh output and draw charts.

usage: build/venv/bin/python -I scripts/analyze.py results/raw/<label> [--out results/<label>] [--base base]

Metrics (per build, per run, then median and min..max over runs):
  median_tok_us   median over grammars of the grammar's mean per-token cost (mask + accept)
  pooled_p50/p99  percentiles over every replayed token of every grammar
  total_s         sum of replay time over the workload
With 2+ builds, ratios are computed run-by-run against --base (run i vs run i) and summarized.
"""
import argparse
import glob
import json
import os
import sys

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# reference palette (dataviz skill): slot 1 blue, slot 2 orange; light surface, text tokens
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT2 = "#52514e"
GRID = "#e4e3df"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": GRID, "axes.labelcolor": TEXT2, "xtick.color": TEXT2, "ytick.color": TEXT2,
    "text.color": TEXT, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
    "axes.spines.top": False, "axes.spines.right": False, "font.size": 9,
    "legend.frameon": False,
})


def subset_of(name):
    if name.startswith("gbnf__"):
        return "gbnf"
    parts = name.split("__")
    return parts[1] if len(parts) > 2 else "other"


def load(raw):
    builds = sorted(d for d in os.listdir(raw) if os.path.isdir(os.path.join(raw, d)))
    per_grammar, tokens = [], []
    for b in builds:
        for f in sorted(glob.glob(os.path.join(raw, b, "run*.jsonl"))):
            run = int(os.path.basename(f)[3:].split(".")[0])
            with open(f) as fh:
                for line in fh:
                    d = json.loads(line)
                    d["build"], d["run"] = b, run
                    per_grammar.append(d)
            tf = f.replace(".jsonl", ".tokens.tsv.gz")
            if os.path.exists(tf):
                t = pd.read_csv(tf, sep="\t", header=None,
                                names=["name", "step", "mask_us", "accept_us", "n_stacks", "n_allowed"])
                t["build"], t["run"] = b, run
                tokens.append(t)
    g = pd.DataFrame(per_grammar)
    t = pd.concat(tokens, ignore_index=True) if tokens else pd.DataFrame()
    if not t.empty:
        t["tok_us"] = t["mask_us"] + t["accept_us"]
    return builds, g, t


def run_metrics(g, t):
    rows = []
    for (b, r), gg in g.groupby(["build", "run"]):
        tt = t[(t.build == b) & (t.run == r)] if not t.empty else pd.DataFrame()
        rows.append({
            "build": b, "run": r,
            "median_tok_us": float(gg["tok_us_mean"].median()),
            "mean_tok_us": float(gg["total_us"].sum() / max(1, gg["tokens"].sum())),
            "pooled_p50_us": float(np.percentile(tt["tok_us"], 50)) if len(tt) else np.nan,
            "pooled_p99_us": float(np.percentile(tt["tok_us"], 99)) if len(tt) else np.nan,
            "total_s": float(gg["total_us"].sum() / 1e6),
            "grammars": len(gg), "tokens": int(gg["tokens"].sum()), "failed": int((~gg["ok"]).sum()),
        })
    return pd.DataFrame(rows)


def fmt_spread(s, digits=1):
    return f"{s.median():.{digits}f} [{s.min():.{digits}f} .. {s.max():.{digits}f}]"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("raw")
    ap.add_argument("--out")
    ap.add_argument("--base", default="base")
    args = ap.parse_args()
    label = os.path.basename(os.path.normpath(args.raw))
    out = args.out or os.path.join("results", label)
    os.makedirs(out, exist_ok=True)

    builds, g, t = load(args.raw)
    if g.empty:
        print("no data", file=sys.stderr)
        return 1
    g["subset"] = g["name"].map(subset_of)
    rm = run_metrics(g, t)
    rm.to_csv(os.path.join(out, "runs.csv"), index=False)

    md = [f"# Benchmark summary: `{label}`", ""]
    meta = os.path.join(args.raw, "meta.txt")
    if os.path.exists(meta):
        md += ["```", open(meta).read().strip(), "```", ""]
    md += ["Values: median over runs [min .. max over runs]. Times in microseconds per token (mask + accept) unless noted.", ""]
    md += ["| build | runs | grammars | tokens | median of per-grammar mean | mean over all tokens | pooled p50 | pooled p99 | total replay (s) |",
           "|---|---|---|---|---|---|---|---|---|"]
    for b in builds:
        r = rm[rm.build == b]
        md.append(f"| {b} | {len(r)} | {r.grammars.iloc[0]} | {r.tokens.iloc[0]} | {fmt_spread(r.median_tok_us)} | "
                  f"{fmt_spread(r.mean_tok_us)} | {fmt_spread(r.pooled_p50_us)} | {fmt_spread(r.pooled_p99_us)} | {fmt_spread(r.total_s, 2)} |")
    md.append("")
    if rm.failed.sum():
        md += [f"**Failures:** {int(rm.failed.sum())} grammar-runs reported ok=false.", ""]

    # per-grammar medians over runs
    pg = g.groupby(["build", "name"]).agg(
        subset=("subset", "first"), tokens=("tokens", "first"), peak_stacks=("peak_stacks", "max"),
        tok_us_mean=("tok_us_mean", "median"), tok_us_p50=("tok_us_p50", "median"),
        tok_us_p99=("tok_us_p99", "median"), tok_us_max=("tok_us_max", "median"),
        accept_us_mean=("accept_us_mean", "median")).reset_index()
    pg.to_csv(os.path.join(out, "per_grammar.csv"), index=False)

    base = args.base if args.base in builds else builds[0]
    pb = pg[pg.build == base].sort_values("tok_us_mean", ascending=False)

    md += [f"## Per subset ({base})", "", "| subset | grammars | median per-grammar mean (us) | median per-grammar p99 (us) | max peak stacks |", "|---|---|---|---|---|"]
    for s, ss in pb.groupby("subset"):
        md.append(f"| {s} | {len(ss)} | {ss.tok_us_mean.median():.0f} | {ss.tok_us_p99.median():.0f} | {ss.peak_stacks.max()} |")
    md.append("")
    md += [f"## Worst 10 grammars by mean per-token cost ({base})", "",
           "| grammar | tokens | mean (us) | p50 (us) | p99 (us) | max (us) | peak stacks |", "|---|---|---|---|---|---|---|"]
    for _, r in pb.head(10).iterrows():
        md.append(f"| {r['name']} | {r.tokens} | {r.tok_us_mean:.0f} | {r.tok_us_p50:.0f} | {r.tok_us_p99:.0f} | {r.tok_us_max:.0f} | {r.peak_stacks} |")
    md.append("")
    with open(os.path.join(out, "worst10.txt"), "w") as f:
        f.write("\n".join(pb.head(10)["name"]) + "\n")
    med_name = pb.iloc[len(pb) // 2]["name"]
    md += [f"Median grammar by mean per-token cost: `{med_name}`", ""]

    # A/B ratios run by run
    others = [b for b in builds if b != base]
    if others:
        md += ["## A/B ratios (other / base, run i vs run i; < 1 is faster)", "",
               "| build | median of per-grammar mean | mean over all tokens | pooled p99 | total |", "|---|---|---|---|---|"]
        for o in others:
            a = rm[rm.build == base].set_index("run")
            c = rm[rm.build == o].set_index("run")
            j = a.join(c, lsuffix="_a", rsuffix="_b", how="inner")
            ratio = lambda m: fmt_spread(j[m + "_b"] / j[m + "_a"], 3)
            md.append(f"| {o} | {ratio('median_tok_us')} | {ratio('mean_tok_us')} | {ratio('pooled_p99_us')} | {ratio('total_s')} |")
        md.append("")

    # charts
    cdir = os.path.join(out, "charts")
    os.makedirs(cdir, exist_ok=True)

    if not t.empty:
        fig, ax = plt.subplots(figsize=(6.4, 3.6))
        for i, b in enumerate(builds):
            x = np.sort(t[(t.build == b) & (t.run == 1)]["tok_us"].values) / 1000.0
            ax.plot(x, np.linspace(0, 1, len(x)), color=SERIES[i % 4], linewidth=2, label=b)
        ax.set_xscale("log")
        ax.set_xlabel("grammar cost per token, ms (full-vocab mask + accept), log scale")
        ax.set_ylabel("fraction of tokens")
        ax.set_title("Per-token grammar cost, all replayed tokens (run 1)", loc="left", color=TEXT)
        if len(builds) > 1:
            ax.legend(loc="lower right")
        fig.tight_layout()
        fig.savefig(os.path.join(cdir, "cdf_per_token.png"), dpi=150)
        plt.close(fig)

    fig, ax = plt.subplots(figsize=(6.4, 3.6))
    for i, b in enumerate(builds):
        y = np.sort(pg[pg.build == b]["tok_us_mean"].values)[::-1] / 1000.0
        ax.plot(np.arange(1, len(y) + 1), y, color=SERIES[i % 4], linewidth=2, label=b)
    ax.set_yscale("log")
    ax.set_xlabel("grammar rank (most expensive first)")
    ax.set_ylabel("mean ms per token, log scale")
    ax.set_title("Mean grammar cost per token, per grammar", loc="left", color=TEXT)
    if len(builds) > 1:
        ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(os.path.join(cdir, "per_grammar_rank.png"), dpi=150)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(6.4, 3.6))
    ax.scatter(pb.peak_stacks, pb.tok_us_mean / 1000.0, s=18, color=SERIES[0], edgecolors=SURFACE, linewidths=1)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("peak number of grammar stacks, log scale")
    ax.set_ylabel("mean ms per token, log scale")
    ax.set_title(f"Grammar cost vs stack count ({base})", loc="left", color=TEXT)
    fig.tight_layout()
    fig.savefig(os.path.join(cdir, "stacks_vs_cost.png"), dpi=150)
    plt.close(fig)

    for o in others:
        m = pg[pg.build == base].set_index("name")[["tok_us_mean"]].join(
            pg[pg.build == o].set_index("name")[["tok_us_mean"]], rsuffix="_o", how="inner")
        sp = np.sort((m.tok_us_mean / m.tok_us_mean_o).values)
        fig, ax = plt.subplots(figsize=(6.4, 3.6))
        ax.bar(np.arange(len(sp)), sp, width=0.8, color=SERIES[1])
        ax.axhline(1.0, color=TEXT2, linewidth=1)
        ax.set_xlabel("grammar (sorted)")
        ax.set_ylabel(f"speedup ({base} / {o})")
        ax.set_title(f"Per-grammar speedup of {o} over {base} (median over runs)", loc="left", color=TEXT)
        fig.tight_layout()
        fig.savefig(os.path.join(cdir, f"speedup_{o}.png"), dpi=150)
        plt.close(fig)

    md += ["## Charts", ""] + [f"![{os.path.basename(p)}](charts/{os.path.basename(p)})" for p in sorted(glob.glob(os.path.join(cdir, "*.png")))]
    with open(os.path.join(out, "summary.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md[:40]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
