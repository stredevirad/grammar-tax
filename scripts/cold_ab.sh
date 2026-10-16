#!/usr/bin/env bash
# Cold-start A/B: every grammar replayed alone in a fresh process (like one llama-cli generation per
# process), builds interleaved per grammar (A,B then B,A per round). Output compatible with ab_stats.py.
# usage: scripts/cold_ab.sh LABEL NAMES_FILE ROUNDS BUILD_A BUILD_B ...
set -euo pipefail
cd "$(dirname "$0")/.."
LABEL=$1; NAMES=$2; R=$3; shift 3; B=("$@")
OUT=results/raw/$LABEL; TMP=build/tmp-cold-$LABEL; mkdir -p "$OUT" "$TMP"
grep -v '^#' data/workload.tsv | sed "s/{model}/qwen05/" | awk -F'\t' 'NR==FNR{w[$1]=1; next} ($1 in w)' "$NAMES" - > "$TMP/manifest.tsv"
echo "date=$(date -Is) kernel=$(uname -r) rounds=$R grammars=$(wc -l < "$TMP/manifest.tsv") mode=cold (one grammar per process)" > "$OUT/meta.txt"
for i in $(seq 1 "$R"); do
    if [ $((i % 2)) = 1 ]; then order=("${B[@]}"); else order=(); for ((x=${#B[@]}-1; x>=0; x--)); do order+=("${B[$x]}"); done; fi
    while IFS= read -r line; do
        printf '%s\n' "$line" > "$TMP/one.tsv"
        for b in "${order[@]}"; do
            n=$(basename "$b"); n=${n#bench-}; mkdir -p "$OUT/$n"
            "$b/bin/grammar_replay" --vocab models/qwen2.5-0.5b-instruct-q4_k_m.gguf --manifest "$TMP/one.tsv" \
                --json "$TMP/one.jsonl" --tokens "$TMP/tok" > /dev/null 2>&1
            cat "$TMP/one.jsonl" >> "$OUT/$n/run$i.jsonl"
            cat "$TMP/tok" >> "$TMP/$n.run$i.tokens"
        done
    done < "$TMP/manifest.tsv"
    for b in "${B[@]}"; do n=$(basename "$b"); n=${n#bench-}; gzip -c -9 "$TMP/$n.run$i.tokens" > "$OUT/$n/run$i.tokens.tsv.gz"; done
    echo "round $i done $(date +%T)"
done
rm -rf "$TMP"
