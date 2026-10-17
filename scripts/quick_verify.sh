#!/usr/bin/env bash
# Quick independent check of the headline result (19.5 min on 4 vCPUs with no ccache, see README.md):
#   1. build upstream at the pinned SHA and the final series (patches 0001-0003), grammar_replay only
#   2. bit-exact mask check on every step of every recorded trace, both vocabs (scripts/check_masks.sh)
#   3. 3-run interleaved A/B on the 20-grammar subset in data/quick_subset.txt (includes o46322)
#   4. print the paired speedup with a bootstrap 95% CI (scripts/ab_stats.py)
# Everything goes to build/qv/ (ignored by git). Exits nonzero if any mask differs.
# Needs models/ (scripts/fetch_models.sh) and build/venv (see README.md), or PY=<python with numpy+pandas>.
#
# usage: scripts/quick_verify.sh
set -euo pipefail
cd "$(dirname "$0")/.."
QV=build/qv
PY=${PY:-build/venv/bin/python}
T0=$(date +%s)
step() { echo "== $1 ($(($(date +%s) - T0)) s elapsed)"; }

[ -f third_party/llama.cpp/CMakeLists.txt ] || { echo "submodule missing: git submodule update --init" >&2; exit 1; }
for m in models/qwen2.5-0.5b-instruct-q4_k_m.gguf models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf; do
    [ -s "$m" ] || { echo "missing $m: run scripts/fetch_models.sh" >&2; exit 1; }
done
"$PY" -c "import numpy, pandas" 2> /dev/null || { echo "need numpy+pandas: see README.md (build/venv) or set PY=" >&2; exit 1; }
mkdir -p "$QV"

step "build base and final"
scripts/apply_patches.sh "$QV/src-final" > "$QV/apply_patches.log"
tail -1 "$QV/apply_patches.log"
TARGETS=grammar_replay scripts/build_variant.sh third_party/llama.cpp "$QV/bench-base"
TARGETS=grammar_replay scripts/build_variant.sh "$QV/src-final" "$QV/bench-final"

step "bit-exact mask check, all traces, both vocabs"
rm -rf "$QV/check" "$QV"/masks.*.tsv.gz
scripts/check_masks.sh "$QV/bench-base" "$QV/bench-final" "$QV/check"

step "3-run interleaved A/B on data/quick_subset.txt"
rm -rf "$QV/raw"
scripts/run_bench.sh -n 3 -l qv -s data/quick_subset.txt -o "$QV/raw" "$QV/bench-base" "$QV/bench-final"

step "paired statistics"
"$PY" -I scripts/ab_stats.py "$QV/raw" --a base --b final --out "$QV" > "$QV/ab_stats.log"
sed -n '/^| statistic/,$p' "$QV/ab_base_vs_final.md"
echo
echo "this machine: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2-)"
echo "study machine (results/ab-final, 5 runs): ~1.43x on this subset, 1.470x on all 206 grammars;"
echo "a different CPU gave 1.545x on this subset (results/quick-verify). Ratios depend on the CPU."
step "done"
