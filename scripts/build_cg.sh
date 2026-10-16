#!/usr/bin/env bash
# Build grammar_replay for callgrind (GGML_NATIVE=OFF: valgrind 3.22 cannot decode AVX-512, D1.7).
# usage: scripts/build_cg.sh SRC_DIR OUT_DIR
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S bench -B "$2" -DCMAKE_BUILD_TYPE=Release -DLLAMA_SRC="$(realpath "$1")" -DGGML_NATIVE=OFF \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache > "$2.configure.log" 2>&1
cmake --build "$2" -j4 --target grammar_replay > "$2.build.log" 2>&1 || { grep error "$2.build.log" | head; exit 1; }
echo "built $2"
