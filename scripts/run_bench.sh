#!/usr/bin/env bash
# Run the replay workload N times over one or more builds, interleaved A/B in the same session.
#
# usage: scripts/run_bench.sh [-n RUNS] [-l LABEL] [-m MODEL_TAG] [-f FILTER] [-j JOBS] BUILD_DIR [BUILD_DIR...]
#   -n  runs per build (default 5). Run order alternates A,B / B,A to cancel drift.
#   -l  label for results/raw/<label>/ (default: date-time)
#   -m  trace/vocab tag: qwen05 (default) or tinyllama
#   -f  only grammars whose name contains FILTER
#   -j  grammars replayed in parallel (default 1 = quietest timing; >1 shards the manifest)
# Each build is named after its dir (build/bench-base -> base). Outputs per build and run:
#   results/raw/<label>/<name>/run<i>.jsonl         per-grammar stats (grammar_replay --json)
#   results/raw/<label>/<name>/run<i>.tokens.tsv.gz per-token times
#   results/raw/<label>/<name>/masks.tsv.gz         mask hashes (run 1 only), compared across builds
set -euo pipefail
cd "$(dirname "$0")/.."

RUNS=5; LABEL=$(date +%Y%m%d-%H%M%S); TAG=qwen05; FILTER=""; JOBS=1
while getopts "n:l:m:f:j:" o; do
    case $o in
        n) RUNS=$OPTARG ;; l) LABEL=$OPTARG ;; m) TAG=$OPTARG ;; f) FILTER=$OPTARG ;; j) JOBS=$OPTARG ;;
        *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { echo "need at least one build dir" >&2; exit 2; }
BUILDS=("$@")

case $TAG in
    qwen05)    VOCAB=models/qwen2.5-0.5b-instruct-q4_k_m.gguf ;;
    tinyllama) VOCAB=models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf ;;
    *) echo "unknown model tag $TAG" >&2; exit 2 ;;
esac

OUT=results/raw/$LABEL
TMP=build/tmp-bench-$LABEL
mkdir -p "$OUT" "$TMP"

# manifest for this model: only grammars whose trace exists
grep -v '^#' data/workload.tsv | sed "s/{model}/$TAG/" | while IFS=$'\t' read -r n g t; do
    if [ -s "$t" ] && { [ -z "$FILTER" ] || [[ $n == *"$FILTER"* ]]; }; then printf '%s\t%s\t%s\n' "$n" "$g" "$t"; fi
done > "$TMP/manifest.tsv"
echo "grammars: $(wc -l < "$TMP/manifest.tsv"), builds: ${BUILDS[*]}, runs: $RUNS, jobs: $JOBS, out: $OUT"
split -n l/"$JOBS" -d "$TMP/manifest.tsv" "$TMP/shard."

{
    echo "date=$(date -Is) host=$(hostname) kernel=$(uname -r) nproc=$(nproc) runs=$RUNS jobs=$JOBS model=$TAG filter=$FILTER"
    grep -m1 "model name" /proc/cpuinfo
    for b in "${BUILDS[@]}"; do echo "build $b: $(git -C "$(grep LLAMA_SRC:PATH "$b/CMakeCache.txt" | cut -d= -f2)" rev-parse HEAD 2>/dev/null) $(git -C "$(grep LLAMA_SRC:PATH "$b/CMakeCache.txt" | cut -d= -f2)" diff --stat HEAD 2>/dev/null | tail -1)"; done
} > "$OUT/meta.txt"

run_one() { # build run
    local b=$1 i=$2 name; name=$(basename "$b"); name=${name#bench-}
    mkdir -p "$OUT/$name"
    local pids=() k=0
    for s in "$TMP"/shard.*; do
        local extra=()
        [ "$i" = 1 ] && extra=(--check "$TMP/$name.masks.$k")
        "$b/bin/grammar_replay" --vocab "$VOCAB" --manifest "$s" --json "$TMP/$name.run$i.$k.jsonl" \
            --tokens "$TMP/$name.run$i.$k.tokens" "${extra[@]}" &
        pids+=($!); k=$((k + 1))
    done
    local rc=0
    for p in "${pids[@]}"; do wait "$p" || rc=$?; done
    cat "$TMP/$name.run$i".*.jsonl > "$OUT/$name/run$i.jsonl"
    cat "$TMP/$name.run$i".*.tokens | gzip -9 > "$OUT/$name/run$i.tokens.tsv.gz"
    [ "$i" = 1 ] && cat "$TMP/$name.masks".* | gzip -9 > "$OUT/$name/masks.tsv.gz"
    echo "  run $i $name done (rc=$rc) $(date +%T)"
}

for i in $(seq 1 "$RUNS"); do
    if [ $((i % 2)) = 1 ]; then order=("${BUILDS[@]}"); else order=(); for ((x=${#BUILDS[@]}-1; x>=0; x--)); do order+=("${BUILDS[$x]}"); done; fi
    for b in "${order[@]}"; do run_one "$b" "$i"; done
done

# bit-exact mask comparison against the first build
first=$(basename "${BUILDS[0]}"); first=${first#bench-}
for b in "${BUILDS[@]:1}"; do
    name=$(basename "$b"); name=${name#bench-}
    if cmp -s <(zcat "$OUT/$first/masks.tsv.gz" | sort) <(zcat "$OUT/$name/masks.tsv.gz" | sort); then
        echo "MASKS IDENTICAL: $first vs $name ($(zcat "$OUT/$first/masks.tsv.gz" | wc -l) steps)" | tee -a "$OUT/meta.txt"
    else
        echo "MASKS DIFFER: $first vs $name" | tee -a "$OUT/meta.txt"
    fi
done
rm -rf "$TMP"
