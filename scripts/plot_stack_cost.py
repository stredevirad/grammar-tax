#!/usr/bin/env python3
"""Per-step cost vs live stacks (all baseline runs pooled): median and IQR per stack-count bucket.
usage: build/venv/bin/python -I scripts/plot_stack_cost.py results/raw/baseline-qwen05/base results/baseline-qwen05/charts/step_cost_vs_stacks.png
"""
import glob, sys
import numpy as np, pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SURFACE, TEXT, TEXT2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
BLUE, ORANGE = "#2a78d6", "#eb6834"
plt.rcParams.update({"figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
                     "axes.edgecolor": GRID, "axes.labelcolor": TEXT2, "xtick.color": TEXT2, "ytick.color": TEXT2,
                     "text.color": TEXT, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
                     "axes.spines.top": False, "axes.spines.right": False, "font.size": 9, "legend.frameon": False})
fr = []
for f in sorted(glob.glob(sys.argv[1] + "/run*.tokens.tsv.gz")):
    t = pd.read_csv(f, sep="\t", header=None, names=["name", "step", "mask", "acc", "ns", "na"]).drop_duplicates(["name", "step"])
    fr.append(t)
t = pd.concat(fr)
t["c"] = (t["mask"] + t.acc) / 1000
t["open"] = t.na / 151936 > 0.5
bins = [0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 100, 150, 250, 400, 700]
fig, ax = plt.subplots(figsize=(6.4, 3.8))
for sel, col, lab in [(~t.open, BLUE, "steps where <= 50% of the vocab is allowed"), (t.open, ORANGE, "steps where > 50% is allowed (e.g. inside a JSON string)")]:
    s = t[sel].copy()
    s["b"] = pd.cut(s.ns, bins)
    g = s.groupby("b", observed=True).agg(x=("ns", "median"), m=("c", "median"), lo=("c", lambda v: v.quantile(.25)), hi=("c", lambda v: v.quantile(.75)), n=("c", "size"))
    g = g[g.n >= 25]
    ax.fill_between(g.x, g.lo, g.hi, color=col, alpha=0.18, linewidth=0)
    ax.plot(g.x, g.m, color=col, linewidth=2, marker="o", markersize=4, label=lab)
xs = np.array([1, 600])
ax.plot(xs, 16.0 + 2.3 * xs, color=TEXT2, linewidth=1, linestyle="--", label="16 ms + 2.3 ms x stacks (fits the blue medians within ~5%)")
ax.set_xscale("log"); ax.set_yscale("log")
ax.set_xlabel("live grammar stacks at this step, log scale")
ax.set_ylabel("ms for this step (mask + accept), log scale")
ax.set_title("Per-step grammar cost vs live stacks (5 runs pooled; line = median, band = IQR)", loc="left", color=TEXT)
ax.legend(loc="upper left", fontsize=8)
fig.tight_layout()
fig.savefig(sys.argv[2], dpi=150)
