#!/usr/bin/env bash
# End-to-end generation with the real model, to put grammar cost next to model compute.
# For each grammar in the e2e subset, runs grammar_record twice (4 threads, one job at a time):
#   --mode first    : full-vocab mask every step (what speculative-draft sampling does)
#   --mode resample : mirrors common_sampler_sample: check the chosen token, full mask only on reject
# usage: scripts/run_e2e.sh [BUILD_DIR=build/bench-base] [LABEL=base] [MODEL_TAG=qwen05]
# subset: data/e2e_subset.txt (all gbnf__* + the 2 first schemas of each JSONSchemaBench subset)
# output: results/e2e/<label>/<model>/<name>.<mode>.txt (trace header has the timings)
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=${1:-build/bench-base}; LABEL=${2:-base}; TAG=${3:-qwen05}
case $TAG in
    qwen05)    MODEL=models/qwen2.5-0.5b-instruct-q4_k_m.gguf ;;
    tinyllama) MODEL=models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf ;;
esac
if [ ! -s data/e2e_subset.txt ]; then
    grep -v '^#' data/workload.tsv | cut -f1 | awk -F'__' '$1=="gbnf"{print; next} {if (c[$2]++ < 2) print}' > data/e2e_subset.txt
fi
OUT=results/e2e/$LABEL/$TAG
mkdir -p "$OUT"
while read -r name; do
    g=$(awk -F'\t' -v n="$name" '$1==n{print $2}' data/workload.tsv)
    for mode in first resample; do
        o=$OUT/$name.$mode.txt
        [ -s "$o" ] && continue
        "$BUILD/bin/grammar_record" -m "$MODEL" -g "$g" -p "data/prompts/$name.txt" -o "$o" -t 4 \
            --seed 1234 --max-tokens 256 --max-seconds 120 --mode $mode > /dev/null 2> "$o.err" || echo "FAIL $name $mode"
        rm -f "$o.out.txt"; [ -s "$o.err" ] || rm -f "$o.err"
    done
    echo "done $name"
done < data/e2e_subset.txt
