#!/usr/bin/env python3
"""Paired A/B statistics for interleaved run_bench.sh output.

usage: build/venv/bin/python -I scripts/ab_stats.py results/raw/<label> --a base --b <name> [--out results/<label>]
       [--boot 2000] [--seed 1]

Pairing: grammar g, run i of A is compared with grammar g, run i of B (run_bench.sh interleaves
A,B / B,A so run i of both builds ran back to back).

Reported (speedup = A time / B time, > 1 means B is faster):
  per grammar : geometric mean over runs of the paired ratio of mean per-token cost, plus min..max over runs
  workload    : geometric mean of the per-grammar speedups, with a bootstrap 95% CI over grammars;
                ratio B/A of the median per-token cost (median over grammars of each grammar's mean);
                ratio B/A of the pooled p99 over all tokens; ratio B/A of total time.
Bootstrap: resample grammars with replacement (the grammar is the unit; runs stay paired), recompute
each statistic per run, take the median over runs, repeat; CI = 2.5 / 97.5 percentiles.
"""
import argparse
import glob
import json
import os
import sys

import numpy as np
import pandas as pd


def load_build(raw, b):
    per, tok = {}, {}
    for f in sorted(glob.glob(os.path.join(raw, b, "run*.jsonl"))):
        run = int(os.path.basename(f)[3:].split(".")[0])
        d = pd.read_json(f, lines=True).drop_duplicates("name").set_index("name")
        per[run] = d
        tf = f.replace(".jsonl", ".tokens.tsv.gz")
        if os.path.exists(tf):
            t = pd.read_csv(tf, sep="\t", header=None, names=["name", "step", "mask_us", "accept_us", "n_stacks", "n_allowed"])
            t = t.drop_duplicates(["name", "step"])
            t["tok_us"] = t["mask_us"] + t["accept_us"]
            tok[run] = {n: g["tok_us"].values for n, g in t.groupby("name")}
    return per, tok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("raw")
    ap.add_argument("--a", default="base")
    ap.add_argument("--b", required=True)
    ap.add_argument("--out")
    ap.add_argument("--boot", type=int, default=2000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    label = os.path.basename(os.path.normpath(args.raw))
    out = args.out or os.path.join("results", label)
    os.makedirs(out, exist_ok=True)

    pa, ta = load_build(args.raw, args.a)
    pb, tb = load_build(args.raw, args.b)
    runs = sorted(set(pa) & set(pb))
    names = sorted(set.intersection(*[set(pa[r].index) & set(pb[r].index) for r in runs]))
    G, R = len(names), len(runs)

    # matrices [grammar, run]
    A = np.array([[pa[r].loc[n, "tok_us_mean"] for r in runs] for n in names])
    B = np.array([[pb[r].loc[n, "tok_us_mean"] for r in runs] for n in names])
    TA = np.array([[pa[r].loc[n, "total_us"] for r in runs] for n in names])
    TB = np.array([[pb[r].loc[n, "total_us"] for r in runs] for n in names])
    ratio = A / B
    per_g = np.exp(np.log(ratio).mean(axis=1))

    have_tok = all(r in ta and r in tb for r in runs)

    def stats(idx):
        geo = float(np.exp(np.log(per_g[idx]).mean()))
        med = float(np.median([np.median(B[idx, j]) / np.median(A[idx, j]) for j in range(R)]))
        tot = float(np.median([TB[idx, j].sum() / TA[idx, j].sum() for j in range(R)]))
        p99 = np.nan
        if have_tok:
            vals = []
            for j, r in enumerate(runs):
                xa = np.concatenate([ta[r][names[i]] for i in idx])
                xb = np.concatenate([tb[r][names[i]] for i in idx])
                vals.append(np.percentile(xb, 99) / np.percentile(xa, 99))
            p99 = float(np.median(vals))
        return geo, med, p99, tot

    point = stats(np.arange(G))
    rng = np.random.default_rng(args.seed)
    boots = np.array([stats(rng.integers(0, G, G)) for _ in range(args.boot)])
    lo = np.nanpercentile(boots, 2.5, axis=0)
    hi = np.nanpercentile(boots, 97.5, axis=0)

    res = {
        "label": label, "a": args.a, "b": args.b, "grammars": G, "runs": R, "bootstrap": args.boot,
        "geomean_speedup": point[0], "geomean_speedup_ci": [lo[0], hi[0]],
        "median_cost_ratio": point[1], "median_cost_ratio_ci": [lo[1], hi[1]],
        "p99_ratio": point[2], "p99_ratio_ci": [lo[2], hi[2]],
        "total_ratio": point[3], "total_ratio_ci": [lo[3], hi[3]],
        "grammars_faster": int((per_g > 1.0).sum()), "grammars_slower": int((per_g < 1.0).sum()),
        "per_grammar_speedup_min": float(per_g.min()), "per_grammar_speedup_max": float(per_g.max()),
    }
    with open(os.path.join(out, f"ab_{args.b}.json"), "w") as f:
        json.dump(res, f, indent=1)

    pgt = pd.DataFrame({"name": names, "speedup_geomean": per_g, "speedup_min": ratio.min(axis=1),
                        "speedup_max": ratio.max(axis=1), "a_tok_us_median": np.median(A, axis=1),
                        "b_tok_us_median": np.median(B, axis=1)}).sort_values("speedup_geomean")
    pgt.to_csv(os.path.join(out, f"ab_{args.b}_per_grammar.csv"), index=False)

    pct = lambda x: f"{100 * (x - 1):+.1f}%"
    md = [f"## Paired A/B: `{args.b}` vs `{args.a}` ({label})", "",
          f"{G} grammars x {R} interleaved runs; bootstrap over grammars, {args.boot} resamples, 95% CI.", "",
          "| statistic | value | 95% CI |", "|---|---|---|",
          f"| geometric-mean per-grammar speedup ({args.a} / {args.b}) | **{point[0]:.3f}x** | {lo[0]:.3f} .. {hi[0]:.3f} |",
          f"| median per-token cost, {args.b} / {args.a} | **{point[1]:.3f}** ({pct(point[1])}) | {lo[1]:.3f} .. {hi[1]:.3f} |",
          f"| pooled p99, {args.b} / {args.a} | **{point[2]:.3f}** ({pct(point[2])}) | {lo[2]:.3f} .. {hi[2]:.3f} |",
          f"| total replay time, {args.b} / {args.a} | {point[3]:.3f} ({pct(point[3])}) | {lo[3]:.3f} .. {hi[3]:.3f} |",
          "",
          f"Per-grammar speedup: {res['grammars_faster']} faster, {res['grammars_slower']} slower; "
          f"range {per_g.min():.3f}x .. {per_g.max():.3f}x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):", ""]
    for _, r in pd.concat([pgt.head(5), pgt.tail(5)]).iterrows():
        md.append(f"- `{r['name']}`: {r.speedup_geomean:.3f}x [{r.speedup_min:.3f} .. {r.speedup_max:.3f}], "
                  f"{r.a_tok_us_median / 1000:.1f} -> {r.b_tok_us_median / 1000:.1f} ms/token")
    text = "\n".join(md) + "\n"
    with open(os.path.join(out, f"ab_{args.b}.md"), "w") as f:
        f.write(text)
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
