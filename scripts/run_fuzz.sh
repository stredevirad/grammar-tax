#!/usr/bin/env bash
# Differential random-walk test (bench/grammar_fuzz.cpp) over grammars/*.gbnf + bench/fuzz_grammars/*.gbnf,
# with both vocabs. Compare two builds with: scripts/run_fuzz.sh compare LABEL_A LABEL_B
# usage: scripts/run_fuzz.sh run BUILD_DIR LABEL [SEED=1]     (also runs the scripted cases in bench/fuzz_grammars/cases.tsv)
#        scripts/run_fuzz.sh cases BUILD_DIR LABEL           (scripted cases only)
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "$1" = run ]; then
    B=$2; L=$3; SEED=${4:-1}
    O=results/fuzz/$L; mkdir -p "$O"
    "$B/bin/grammar_fuzz" --vocab models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf --manifest data/fuzz_manifest.tsv \
        --out "$O/tinyllama.tsv" --coverage "$O/tinyllama.cov.jsonl" --walks 24 --steps 64 --seed "$SEED"
    "$B/bin/grammar_fuzz" --vocab models/qwen2.5-0.5b-instruct-q4_k_m.gguf --manifest data/fuzz_manifest.tsv \
        --out "$O/qwen05.tsv" --coverage "$O/qwen05.cov.jsonl" --walks 8 --steps 48 --seed "$SEED"
    for v in tinyllama qwen05; do
        [ $v = tinyllama ] && M=models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf || M=models/qwen2.5-0.5b-instruct-q4_k_m.gguf
        "$B/bin/grammar_fuzz" --vocab "$M" --cases bench/fuzz_grammars/cases.tsv --out "$O/cases.$v.tsv"
    done
    gzip -f -9 "$O/tinyllama.tsv" "$O/qwen05.tsv"
    echo "fuzz $L: $(zcat "$O/tinyllama.tsv.gz" | wc -l) steps tinyllama, $(zcat "$O/qwen05.tsv.gz" | wc -l) steps qwen05"
elif [ "$1" = cases ]; then
    B=$2; O=results/fuzz/$3; mkdir -p "$O"
    "$B/bin/grammar_fuzz" --vocab models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf --cases bench/fuzz_grammars/cases.tsv --out "$O/cases.tinyllama.tsv"
    "$B/bin/grammar_fuzz" --vocab models/qwen2.5-0.5b-instruct-q4_k_m.gguf --cases bench/fuzz_grammars/cases.tsv --out "$O/cases.qwen05.tsv"
elif [ "$1" = compare ]; then
    A=results/fuzz/$2; B=results/fuzz/$3; rc=0
    for v in tinyllama qwen05; do
        if cmp -s "$A/cases.$v.tsv" "$B/cases.$v.tsv"; then
            echo "CASES IDENTICAL $v: $2 vs $3 ($(wc -l < "$A/cases.$v.tsv") scripted steps)"
        else
            echo "CASES DIFFER $v: $2 vs $3"; diff "$A/cases.$v.tsv" "$B/cases.$v.tsv" | head -4; rc=1
        fi
        if cmp -s <(zcat "$A/$v.tsv.gz") <(zcat "$B/$v.tsv.gz"); then
            echo "FUZZ IDENTICAL $v: $2 vs $3 ($(zcat "$A/$v.tsv.gz" | wc -l) steps)"
        else
            echo "FUZZ DIFFER $v: $2 vs $3, first difference:"
            diff <(zcat "$A/$v.tsv.gz") <(zcat "$B/$v.tsv.gz") | head -4
            rc=1
        fi
    done
    exit $rc
fi
