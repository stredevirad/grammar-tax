#!/usr/bin/env bash
# Profile grammar_replay on a set of grammars.
# usage: scripts/profile.sh LABEL NAMES_FILE [CG_BUILD=build/cg-base] [PERF_BUILD=build/bench-base] [MODEL_TAG=qwen05]
#   callgrind: instruction counts, collection limited to llama_grammar_apply_impl / accept_impl
#              (needs a GGML_NATIVE=OFF build: valgrind 3.22 cannot decode AVX-512; libllama itself
#              is compiled without -march=native in both builds, so grammar code is identical)
#   perf:      cpu-clock sampling (no HW counters in this VM), on the normal native build
# output: results/profile/<LABEL>/{callgrind.out, callgrind_excl.txt, callgrind_incl.txt, perf_report.txt}
set -euo pipefail
cd "$(dirname "$0")/.."
LABEL=$1; NAMES=$2; CG=${3:-build/cg-base}; PB=${4:-build/bench-base}; TAG=${5:-qwen05}
case $TAG in
    qwen05)    VOCAB=models/qwen2.5-0.5b-instruct-q4_k_m.gguf ;;
    tinyllama) VOCAB=models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf ;;
esac
OUT=results/profile/$LABEL
mkdir -p "$OUT"
grep -v '^#' data/workload.tsv | sed "s/{model}/$TAG/" | awk -F'\t' 'NR==FNR{want[$1]=1; next} ($1 in want)' "$NAMES" - > "$OUT/manifest.tsv"
echo "grammars: $(wc -l < "$OUT/manifest.tsv")"

valgrind --tool=callgrind --callgrind-out-file="$OUT/callgrind.out" \
    --toggle-collect='llama_grammar_apply_impl*' --toggle-collect='llama_grammar_accept_impl*' \
    "$CG/bin/grammar_replay" --vocab "$VOCAB" --manifest "$OUT/manifest.tsv" > "$OUT/callgrind.log" 2>&1
callgrind_annotate --inclusive=no  "$OUT/callgrind.out" > "$OUT/callgrind_excl.txt" 2>/dev/null
callgrind_annotate --inclusive=yes "$OUT/callgrind.out" > "$OUT/callgrind_incl.txt" 2>/dev/null
callgrind_annotate --tree=calling --inclusive=yes "$OUT/callgrind.out" > "$OUT/callgrind_tree.txt" 2>/dev/null

PERF=$(ls /usr/lib/linux-tools/*/perf 2>/dev/null | head -1 || true)
if [ -n "$PERF" ]; then
    "$PERF" record -e cpu-clock -F 1000 -g -o "$OUT/perf.data" -- \
        "$PB/bin/grammar_replay" --vocab "$VOCAB" --manifest "$OUT/manifest.tsv" > /dev/null 2> "$OUT/perf.log" || true
    "$PERF" report -i "$OUT/perf.data" --no-children --percent-limit 0.5 --stdio --sort symbol 2>/dev/null \
        | grep -v '^$' | grep -E '^ +[0-9]' > "$OUT/perf_report.txt" || true
    rm -f "$OUT/perf.data"
fi
grep -E "Collected" "$OUT/callgrind.log"
