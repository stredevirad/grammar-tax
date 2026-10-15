# Phase log

Current status: **Phase 1 in progress** (recording traces). Stop point requested by user: after Phase 2 GATE.

## Phase 0: setup - DONE
- Submodule pinned at `620d222` (see README). Bench wrapper build at `build/bench-base`.
- Tools: ccache, valgrind/callgrind, hyperfine OK; perf software events only (no HW PMU). See DECISIONS D0.5.
- Baseline grammar tests: parser, integration, llama-grammar, json-schema-to-grammar all pass.

## Phase 1: harness - IN PROGRESS
- Done: grammar_record / grammar_replay / schema_to_gbnf tools; workload = 8 built-in .gbnf + 200 JSONSchemaBench schemas (20 x 10 subsets, 0 conversion failures).
- First smoke numbers (Qwen2.5-0.5B, 151k vocab, 2 threads): json.gbnf grammar ~64 ms/token vs decode ~34 ms/token.
- Recording qwen05 traces: 146/208 done (first background job hit the 30 min limit; resumed, script skips finished traces).
- Harness verified end to end (run_bench.sh + analyze.py smoke test; masks identical across repeated runs).
- First callgrind profile (json.gbnf): decode_utf8 + per-token vector alloc/free ~44%, match_char ~26%, reject_candidates_for_stack self ~19%, advance_stack ~1.6%.
- Next: finish recording, baseline replay x5, e2e run, llguidance attempt, results/baseline.md.
