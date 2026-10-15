#!/usr/bin/env python3
"""Summarize scripts/run_e2e.sh output.

usage: build/venv/bin/python -I scripts/summarize_e2e.py results/e2e/<label>/<model> [--out file.md]

Per grammar and mode (first = full mask every step, resample = default common_sampler path):
generated tokens, steps that needed the full mask, ms/token for model decode, grammar, sampler chain.
"""
import argparse
import glob
import os
import sys


def parse(path):
    with open(path) as f:
        head = f.readline().strip()
    kv = dict(x.split("=", 1) for x in head.lstrip("# ").split() if "=" in x)
    for k in list(kv):
        try:
            kv[k] = float(kv[k])
        except ValueError:
            pass
    return kv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("--out")
    args = ap.parse_args()

    rows = {}
    for p in sorted(glob.glob(os.path.join(args.dir, "*.txt"))):
        base = os.path.basename(p)[:-4]
        name, mode = base.rsplit(".", 1)
        rows.setdefault(name, {})[mode] = parse(p)

    md = ["| grammar | tokens (first / resample) | full-mask steps on resample path | decode ms/tok | grammar ms/tok, full mask every step | grammar ms/tok, resample path | chain ms/tok |",
          "|---|---|---|---|---|---|---|"]
    agg = {"n_first": 0, "n_res": 0, "full_res": 0, "dec_ms": 0.0, "dec_n": 0, "g_first": 0.0, "g_res": 0.0, "chain_res": 0.0}
    fracs, ratio_first, ratio_res = [], [], []
    for name, m in rows.items():
        if "first" not in m or "resample" not in m:
            continue
        f, r = m["first"], m["resample"]
        nf, nr = max(1, f["n_gen"]), max(1, r["n_gen"])
        dec = (f["t_decode_ms"] + r["t_decode_ms"]) / max(1, (f["n_gen"] - 1) + (r["n_gen"] - 1))
        gf = f["t_grammar_ms"] / nf
        gr = r["t_grammar_ms"] / nr
        frac = r["n_full_mask"] / nr
        fracs.append(frac)
        ratio_first.append(gf / dec if dec else 0)
        ratio_res.append(gr / dec if dec else 0)
        agg["n_first"] += f["n_gen"]; agg["n_res"] += r["n_gen"]; agg["full_res"] += r["n_full_mask"]
        agg["dec_ms"] += f["t_decode_ms"] + r["t_decode_ms"]; agg["dec_n"] += (f["n_gen"] - 1) + (r["n_gen"] - 1)
        agg["g_first"] += f["t_grammar_ms"]; agg["g_res"] += r["t_grammar_ms"]; agg["chain_res"] += r["t_chain_ms"]
        md.append(f"| {name} | {int(f['n_gen'])} / {int(r['n_gen'])} | {int(r['n_full_mask'])} ({100 * frac:.0f}%) | {dec:.1f} | {gf:.1f} | {gr:.1f} | {r['t_chain_ms'] / nr:.1f} |")

    fracs.sort()
    n = len(fracs)
    dec_all = agg["dec_ms"] / max(1, agg["dec_n"])
    summary = [
        f"Grammars: {n}. Pooled over all of them:",
        "",
        f"- model decode: **{dec_all:.1f} ms/token** (4 threads)",
        f"- grammar with the full mask every step (`grammar_first`, e.g. speculative draft sampling): **{agg['g_first'] / max(1, agg['n_first']):.1f} ms/token** "
        f"= {100 * (agg['g_first'] / max(1, agg['n_first'])) / dec_all:.0f}% of decode time",
        f"- grammar on the default resample path (`common_sampler_sample`): **{agg['g_res'] / max(1, agg['n_res']):.1f} ms/token** "
        f"= {100 * (agg['g_res'] / max(1, agg['n_res'])) / dec_all:.0f}% of decode time",
        f"- steps on the resample path that needed the full mask: **{agg['full_res']:.0f} / {agg['n_res']:.0f} = {100 * agg['full_res'] / max(1, agg['n_res']):.1f}%** "
        f"(per grammar: median {100 * fracs[n // 2]:.0f}%, min {100 * fracs[0]:.0f}%, max {100 * fracs[-1]:.0f}%)",
        f"- sampler chain (top-k 40, temp, dist) on the resample path: {agg['chain_res'] / max(1, agg['n_res']):.1f} ms/token",
        "",
    ]
    text = "\n".join(summary + md) + "\n"
    if args.out:
        with open(args.out, "w") as fh:
            fh.write(text)
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
