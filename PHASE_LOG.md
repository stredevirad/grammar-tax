# Phase log

**Phase 3 IN PROGRESS** (user gave go-ahead on 2026-10-16 with additions: order P1, P2, then skip-advance and cheaper is_eog; P2 edge-case + random-walk differential test; P1 memory cost; who-benefits audit with file:line; #28371 third arm if it applies in ~15 min; bootstrap CIs; fix baseline.md stack-count wording; MIT LICENSE + third-party credits + main branch at the end).

## Phase 0: setup - DONE
- Submodule pinned at `620d222` (see README). Bench wrapper build at `build/bench-base`.
- Tools: ccache, valgrind/callgrind, hyperfine OK; perf software events only (no HW PMU). See DECISIONS D0.5.
- Baseline grammar tests: parser, integration, llama-grammar, json-schema-to-grammar all pass.

## Phase 1: harness - DONE
- Done: grammar_record / grammar_replay / schema_to_gbnf tools; workload = 8 built-in .gbnf + 200 JSONSchemaBench schemas (20 x 10 subsets, 0 conversion failures).
- First smoke numbers (Qwen2.5-0.5B, 151k vocab, 2 threads): json.gbnf grammar ~64 ms/token vs decode ~34 ms/token.
- Recording qwen05 traces: 146/208 done (first background job hit the 30 min limit; resumed, script skips finished traces).
- Harness verified end to end (run_bench.sh + analyze.py smoke test; masks identical across repeated runs).
- First callgrind profile (json.gbnf): decode_utf8 + per-token vector alloc/free ~44%, match_char ~26%, reject_candidates_for_stack self ~19%, advance_stack ~1.6%.
- Recording done: 208 traces (206 distinct grammars, D1.11), 23.4k tokens.
- Baseline replay x5 (j=1, quiet machine): run 1 total 1050 s, run 2 1026 s; median per-grammar mean ~34 ms/token.
- llguidance built in ~2 min; reference replay: 162/198 schemas measurable, median 0.41 ms/token.
- Added: grammar_replay --max-steps (for callgrind), apply_patches.sh, build_variant.sh, run_tests.sh, LEARNING.md part 1, UPSTREAM.md, upstream overlap table in DECISIONS.md.
- Baseline final: median per-grammar cost 31.3 ms/token [29.0..34.1], pooled p99 199 ms [163..251], 5 runs; monotonic VM drift of ~20% across runs (A/B must interleave).
- e2e (28 grammars): decode 27.2 ms/tok; full mask every step 35.6 ms/tok (131% of decode); default resample path 1.5 ms/tok (6%), full mask on 5.1% of steps.

## Phase 2: profile + GATE - DONE (waiting for go-ahead)
- callgrind (40 steps/trace) + perf (full traces) on median9 and worst10: results/profile/.
- Median: decode-related work 58.7% of instructions; malloc/free 25.6% of time. Worst: per-stack sweep 63% of time (memory bound), match_char 14.5%; advance_stack <1% (dedup candidate rejected).
- GATE: PASS. P1 flat code-point buffer (median -20..-30%, ~25 lines); P2 per-stack first-char pre-filter (p99 -40..-60%, ~50 lines). See results/profile.md section 5.
- Next (only after user says go): Phase 3, P1 then P2.

## Phase 3: optimize - IN PROGRESS
- Started 2026-10-16. Plan: tooling (bootstrap CIs, RSS, fuzz diff test) -> P1 -> P2 -> small ideas -> #28371 arm.
