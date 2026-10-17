# PR #28371 memcheck logs

Same command for both, TinyLlama vocab, `english.gbnf`, first 12 trace steps, callgrind-style build
(`GGML_NATIVE=OFF`, valgrind 3.22 cannot decode AVX-512):

```sh
printf 'gbnf__english\tthird_party/llama.cpp/grammars/english.gbnf\tbench/traces/tinyllama/gbnf__english.txt\n' > build/eng-tiny.tsv
valgrind --tool=memcheck --error-limit=no build/cg-<arm>/bin/grammar_replay \
    --vocab models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf --manifest build/eng-tiny.tsv --max-steps 12
```

| arm | log | result |
|---|---|---|
| #28371 head `f000de6` | `memcheck-f000de6.log.gz` (2026-10-16) | `ERROR SUMMARY: 8829 errors from 30 contexts`: 12 "Invalid read of size 4", 14 "Conditional jump or move depends on uninitialised value(s)", 4 "Use of uninitialised value of size 8"; top frames: 20 `_Hashtable::_M_find_before_node`, 8 `_Hashtable::_M_rehash` (the memo cache's table), 2 `llama_grammar_candidates_hash::operator()` |
| baseline (pinned SHA) | `memcheck-base.log` (re-run 2026-10-17) | `ERROR SUMMARY: 0 errors from 0 contexts` |

Details and the cause: DECISIONS.md D3.11, results/REPORT.md section 6.
