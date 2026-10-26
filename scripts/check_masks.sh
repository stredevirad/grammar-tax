#!/usr/bin/env bash
# Bit-exact mask comparison of two builds on every step of every recorded trace (both vocabs).
# usage: scripts/check_masks.sh BUILD_A BUILD_B [OUT_DIR=results/check/<b>]
set -euo pipefail
cd "$(dirname "$0")/.."
A=$1; B=$2; nb=$(basename "$B"); nb=${nb#bench-}; OUT=${3:-results/check/$nb}
mkdir -p "$OUT"; rc=0
for TAG in ${VOCABS:-qwen05 tinyllama}; do
    case $TAG in
        qwen05)    VOCAB=models/qwen2.5-0.5b-instruct-q4_k_m.gguf ;;
        tinyllama) VOCAB=models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf ;;
    esac
    grep -v '^#' data/workload.tsv | sed "s/{model}/$TAG/" > "$OUT/manifest.$TAG.tsv"
    split -n l/4 -d "$OUT/manifest.$TAG.tsv" "$OUT/shard.$TAG."
    # list the shards now: later files (shard.*.<build>.masks/.err) must not be replayed as manifests
    shards=("$OUT"/shard.$TAG.??)
    for X in "$A" "$B"; do
        nx=$(basename "$X"); nx=${nx#bench-}
        if [ "$X" = "$A" ] && [ -s "$OUT/../masks.$nx.$TAG.tsv.gz" ]; then continue; fi
        pids=()
        for s in "${shards[@]}"; do
            "$X/bin/grammar_replay" --vocab "$VOCAB" --manifest "$s" --check "$s.$nx.masks" > /dev/null 2> "$s.$nx.err" & pids+=($!)
        done
        for p in "${pids[@]}"; do wait "$p" || { echo "replay exited nonzero ($nx $TAG)"; rc=1; }; done
        cat "$OUT"/shard.$TAG.??.$nx.err > "$OUT/replay.$nx.$TAG.err"
        cat "$OUT"/shard.$TAG.??.$nx.masks | gzip -9 > "$OUT/../masks.$nx.$TAG.tsv.gz"
    done
    na=$(basename "$A"); na=${na#bench-}
    n=$(zcat "$OUT/../masks.$na.$TAG.tsv.gz" | wc -l)
    if cmp -s <(zcat "$OUT/../masks.$na.$TAG.tsv.gz") <(zcat "$OUT/../masks.$nb.$TAG.tsv.gz"); then
        echo "MASKS IDENTICAL $TAG: $na vs $nb, $n steps" | tee -a "$OUT/result.txt"
    else
        echo "MASKS DIFFER $TAG: $na vs $nb" | tee -a "$OUT/result.txt"; rc=1
        diff <(zcat "$OUT/../masks.$na.$TAG.tsv.gz") <(zcat "$OUT/../masks.$nb.$TAG.tsv.gz") | head -5 || true
    fi
    rm -f "$OUT"/shard.$TAG.*
done
exit $rc
