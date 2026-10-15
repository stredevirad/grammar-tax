#!/usr/bin/env bash
# Record one constrained generation per workload grammar (fixed seed) -> bench/traces/<tag>/<name>.txt
# usage: scripts/record_traces.sh <model.gguf> <tag> [jobs=2] [threads_per_job=2]
set -euo pipefail
cd "$(dirname "$0")/.."
MODEL=$1; TAG=$2; JOBS=${3:-2}; THREADS=${4:-2}
BIN=${BIN:-build/bench-base/bin/grammar_record}
mkdir -p bench/traces/$TAG
grep -v '^#' data/workload.tsv | while IFS=$'\t' read -r name gpath tpath; do
    out=bench/traces/$TAG/$name.txt
    [ -s "$out" ] && continue
    printf '%s\t%s\n' "$name" "$gpath"
done | xargs -P "$JOBS" -L 1 bash -c '
    name=$0; gpath=$1; out=bench/traces/'"$TAG"'/$name.txt
    if '"$BIN"' -m '"$MODEL"' -g "$gpath" -p data/prompts/$name.txt -o "$out" -t '"$THREADS"' --seed 1234 --max-tokens 256 --max-seconds 120 > "$out.log" 2>&1; then
        echo "ok   $name $(tail -1 $out.log)"
    else
        echo "FAIL $name $(tail -1 $out.log)"; rm -f "$out"
    fi'
