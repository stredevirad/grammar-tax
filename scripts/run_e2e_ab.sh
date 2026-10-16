#!/usr/bin/env bash
# Interleaved end-to-end A/B: scripts/run_e2e.sh for each build, R rounds, order A,B then B,A.
# usage: scripts/run_e2e_ab.sh ROUNDS BUILD_A BUILD_B ...
# output: results/e2e/<name>-r<i>/qwen05/  (name = build dir without "bench-")
set -euo pipefail
cd "$(dirname "$0")/.."
R=$1; shift; B=("$@")
for i in $(seq 1 "$R"); do
    if [ $((i % 2)) = 1 ]; then order=("${B[@]}"); else order=(); for ((x=${#B[@]}-1; x>=0; x--)); do order+=("${B[$x]}"); done; fi
    for b in "${order[@]}"; do
        n=$(basename "$b"); n=${n#bench-}
        scripts/run_e2e.sh "$b" "$n-r$i" qwen05 > /dev/null
        echo "round $i $n done $(date +%T)"
    done
done
