#!/usr/bin/env bash
# Configure + build the bench wrapper against a llama.cpp source tree.
# usage: scripts/build_variant.sh SRC_DIR BUILD_DIR [extra cmake args...]
#   e.g. scripts/build_variant.sh build/src-patched build/bench-patched
#        scripts/build_variant.sh build/src-patched build/cg-patched -DGGML_NATIVE=OFF   (for callgrind)
set -euo pipefail
cd "$(dirname "$0")/.."
SRC=$1; OUT=$2; shift 2
cmake -S bench -B "$OUT" -DCMAKE_BUILD_TYPE=Release -DLLAMA_SRC="$(realpath "$SRC")" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache "$@" > "$OUT.configure.log" 2>&1 \
    || { tail -20 "$OUT.configure.log"; exit 1; }
cmake --build "$OUT" -j4 --target grammar_replay grammar_record schema_to_gbnf \
    test-grammar-parser test-grammar-integration test-llama-grammar test-json-schema-to-grammar \
    llama-completion llama-bench > "$OUT.build.log" 2>&1 || { grep -E "error" "$OUT.build.log" | head -20; exit 1; }
echo "built $OUT from $SRC"
