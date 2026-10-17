#!/usr/bin/env bash
# Single-run page-fault check of the cold-start effect (DECISIONS D3.13, D3.15): replay o46322 (600 live
# stacks) in a fresh grammar_replay process per row, with `perf stat -e page-faults` on the whole process.
# Rows: each build alone, alone with GLIBC_TUNABLES (raised mmap/trim thresholds), and after gbnf json in the
# same process. These are single runs, not a paired A/B (for that see scripts/cold_ab.sh).
#
# usage: scripts/cold_faults.sh OUT_DIR BUILD_DIR [BUILD_DIR...]
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=$1; shift
mkdir -p "$OUT"
PERF=${PERF:-$(ls /usr/lib/linux-tools/*/perf 2> /dev/null | head -1)}    # /usr/bin/perf rejects unknown kernels
[ -x "$PERF" ] || { echo "perf not found (set PERF=)" >&2; exit 1; }
VOCAB=models/qwen2.5-0.5b-instruct-q4_k_m.gguf
TUNABLES=glibc.malloc.mmap_threshold=67108864:glibc.malloc.trim_threshold=268435456
grep -P '^jsb__Github_ultra__o46322\t' data/workload.tsv | sed 's/{model}/qwen05/' > "$OUT/alone.tsv"
{ grep -P '^gbnf__json\t' data/workload.tsv; grep -P '^jsb__Github_ultra__o46322\t' data/workload.tsv; } \
    | sed 's/{model}/qwen05/' > "$OUT/after-json.tsv"
echo "date=$(date -Is) kernel=$(uname -r) $(grep -m1 'model name' /proc/cpuinfo)" > "$OUT/meta.txt"
printf 'build\tcase\to46322_ms_per_token\tpage_faults\n' > "$OUT/faults.tsv"
for b in "$@"; do
    n=$(basename "$b"); n=${n#bench-}
    for c in alone tunables after-json; do
        m=alone; [ $c = after-json ] && m=after-json
        env=(); [ $c = tunables ] && env=(GLIBC_TUNABLES=$TUNABLES)
        env "${env[@]}" "$PERF" stat -e page-faults -x, -o "$OUT/$n.$c.perf.txt" -- \
            "$b/bin/grammar_replay" --vocab "$VOCAB" --manifest "$OUT/$m.tsv" --json "$OUT/$n.$c.jsonl" > /dev/null
        ms=$(grep o46322 "$OUT/$n.$c.jsonl" | sed 's/.*"tok_us_mean":\([0-9.]*\).*/\1/')
        pf=$(grep page-faults "$OUT/$n.$c.perf.txt" | cut -d, -f1)
        printf '%s\t%s\t%.1f\t%s\n' "$n" "$c" "$(echo "$ms / 1000" | bc -l)" "$pf" | tee -a "$OUT/faults.tsv"
    done
done
