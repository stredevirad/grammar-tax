#!/usr/bin/env bash
# Callgrind instruction counts for several variants on the median9 and worst10 sets (40 steps per trace),
# 4 at a time. Instruction counts do not depend on machine load, so these may run in parallel.
# usage: scripts/run_callgrind_all.sh base p1 p2 ...   (uses build/cg-<name>)
# output: results/profile/<set>-<name>/ ; summary in results/profile/instructions.tsv
set -euo pipefail
cd "$(dirname "$0")/.."
jobs=()
for v in "$@"; do
    for set in median9 worst10; do
        jobs+=("$set $v")
    done
done
printf '%s\n' "${jobs[@]}" | xargs -P 4 -L 1 bash -c '
    set=$0; v=$1
    OUT=results/profile/$set-$v; mkdir -p "$OUT"
    grep -v "^#" data/workload.tsv | sed "s/{model}/qwen05/" | awk -F"\t" "NR==FNR{w[\$1]=1; next} (\$1 in w)" results/baseline-qwen05/$set.txt - > "$OUT/manifest.tsv"
    valgrind --tool=callgrind --callgrind-out-file="$OUT/callgrind.out" \
        --toggle-collect="llama_grammar_apply_impl*" --toggle-collect="llama_grammar_accept_impl*" \
        build/cg-$v/bin/grammar_replay --vocab models/qwen2.5-0.5b-instruct-q4_k_m.gguf --manifest "$OUT/manifest.tsv" --max-steps 40 \
        --check "$OUT/masks.tsv" > "$OUT/callgrind.log" 2>&1
    callgrind_annotate --inclusive=no "$OUT/callgrind.out" > "$OUT/callgrind_excl.txt" 2>/dev/null
    echo "$set $v $(grep -o "Collected : [0-9]*" "$OUT/callgrind.log" | grep -o "[0-9]*")"
'
{
    echo -e "set\tvariant\tinstructions"
    for v in "$@"; do for set in median9 worst10; do
        printf '%s\t%s\t%s\n' "$set" "$v" "$(grep -o "Collected : [0-9]*" results/profile/$set-$v/callgrind.log | grep -o "[0-9]*")"
    done; done
} > results/profile/instructions.tsv
cat results/profile/instructions.tsv
