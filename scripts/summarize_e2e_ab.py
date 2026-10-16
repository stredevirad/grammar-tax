#!/usr/bin/env python3
"""Compare end-to-end runs from run_e2e_ab.sh.
usage: build/venv/bin/python -I scripts/summarize_e2e_ab.py A B ROUNDS [--out file.md]
Per grammar, mode and round: tokens/s = n_gen / (prompt-excluded time = decode + grammar + chain).
Paired ratio B/A per (grammar, mode, round); summary = geometric mean over grammars (median over rounds),
plus pooled tokens/s (sum tokens / sum time). Token sequences must match (same masks, same seed).
"""
import argparse, glob, os, sys
import numpy as np

def parse(path):
    with open(path) as f:
        head = f.readline().strip(); toks = f.readline().split()
    kv = {}
    for x in head.lstrip("# ").split():
        if "=" in x:
            k, v = x.split("=", 1)
            try: kv[k] = float(v)
            except ValueError: kv[k] = v
    kv["tokens"] = toks
    return kv

ap = argparse.ArgumentParser(); ap.add_argument("a"); ap.add_argument("b"); ap.add_argument("rounds", type=int); ap.add_argument("--out")
args = ap.parse_args()
md = [f"## End to end: `{args.b}` vs `{args.a}` ({args.rounds} interleaved rounds, Qwen2.5-0.5B Q4_K_M, 4 threads)", "",
      "| mode | grammars | tokens/s A (pooled, median over rounds) | tokens/s B | B / A pooled | geomean per-grammar B / A | grammar ms/token A -> B | token sequences identical |", "|---|---|---|---|---|---|---|---|"]
for mode in ["resample", "first"]:
    pooled_a, pooled_b, geo, gms_a, gms_b, same = [], [], [], [], [], True
    for r in range(1, args.rounds + 1):
        fa = sorted(glob.glob(f"results/e2e/{args.a}-r{r}/qwen05/*.{mode}.txt"))
        ta = tb = na = nb = ga = gb = 0.0; ratios = []
        for f in fa:
            g = os.path.basename(f)
            fb = f"results/e2e/{args.b}-r{r}/qwen05/{g}"
            if not os.path.exists(fb): continue
            A, B = parse(f), parse(fb)
            same &= A["tokens"] == B["tokens"]
            sa = A["t_decode_ms"] + A["t_grammar_ms"] + A["t_chain_ms"]; sb = B["t_decode_ms"] + B["t_grammar_ms"] + B["t_chain_ms"]
            ta += sa; tb += sb; na += A["n_gen"]; nb += B["n_gen"]; ga += A["t_grammar_ms"]; gb += B["t_grammar_ms"]
            ratios.append((B["n_gen"] / sb) / (A["n_gen"] / sa))
        pooled_a.append(1000 * na / ta); pooled_b.append(1000 * nb / tb); geo.append(float(np.exp(np.mean(np.log(ratios)))))
        gms_a.append(ga / na); gms_b.append(gb / nb); n = len(ratios)
    md.append(f"| {mode} | {n} | {np.median(pooled_a):.2f} | {np.median(pooled_b):.2f} | {np.median(pooled_b) / np.median(pooled_a):.3f} | "
              f"{np.median(geo):.3f} [{min(geo):.3f} .. {max(geo):.3f}] | {np.median(gms_a):.2f} -> {np.median(gms_b):.2f} | {'yes' if same else 'NO'} |")
text = "\n".join(md) + "\n"
if args.out: open(args.out, "w").write(text)
print(text)
