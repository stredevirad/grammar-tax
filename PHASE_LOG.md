# Phase log

Current status: **Phase 1 in progress** (baseline replay runs 3-5 running; then profiles, e2e, TinyLlama, then Phase 2 write-up and GATE). Stop point requested by user: after Phase 2 GATE.

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
- Recording done: 208 traces (206 distinct grammars, D1.11), 23.4k tokens.
- Baseline replay x5 (j=1, quiet machine): run 1 total 1050 s, run 2 1026 s; median per-grammar mean ~34 ms/token.
- llguidance built in ~2 min; reference replay: 162/198 schemas measurable, median 0.41 ms/token.
- Added: grammar_replay --max-steps (for callgrind), apply_patches.sh, build_variant.sh, run_tests.sh, LEARNING.md part 1, UPSTREAM.md, upstream overlap table in DECISIONS.md.
- Next: profiles (callgrind capped at 40 steps + perf full) on median9 / worst10, e2e subset, TinyLlama record + replay x3, fill results/baseline.md, write results/profile.md, GATE.
