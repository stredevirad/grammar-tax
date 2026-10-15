#!/usr/bin/env bash
# Run the llama.cpp grammar test suite from a build dir (tests read files relative to the source root).
# usage: scripts/run_tests.sh BUILD_DIR
set -uo pipefail
cd "$(dirname "$0")/.."
B=$(realpath "$1")
SRC=$(grep LLAMA_SRC:PATH "$B/CMakeCache.txt" | cut -d= -f2)
fail=0
for t in test-grammar-parser test-grammar-integration test-llama-grammar test-json-schema-to-grammar; do
    ( cd "$SRC" && "$B/bin/$t" > "$B/$t.log" 2>&1 )
    rc=$?
    echo "$t: $([ $rc = 0 ] && echo PASS || echo "FAIL (rc=$rc, see $B/$t.log)")"
    [ $rc = 0 ] || fail=1
done
exit $fail
