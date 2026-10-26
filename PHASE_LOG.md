# Phase log

**ALL PHASES DONE.** Final series: patches/0001 (flat decode buffer) + 0002 (ASCII match table) + 0003 (reuse reject lists). Mask computation 1.470x [1.455, 1.486] per grammar vs upstream (5 interleaved runs, 206 grammars, Qwen vocab), median -32.7%, p99 -17.9%, 206/206 faster, instructions -47% (median set) / -39% (worst set); masks bit-identical on 44,650 trace steps + fuzz. End to end: +1.5% tok/s on the default path, +24.2% with the full mask every step. Report: results/REPORT.md. main branch created from the final state.

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

## Phase 3: optimize - DONE
- Started 2026-10-16. Plan: tooling (bootstrap CIs, RSS, fuzz diff test) -> P1 -> P2 -> small ideas -> #28371 arm.
- Tooling: ab_stats.py (paired per-grammar ratios, bootstrap 95% CI over grammars), replay max_rss_kb, grammar_fuzz (random walks + scripted edge cases, bench/fuzz_grammars/), check_masks.sh, run_fuzz.sh, make_patch.sh, build_cg.sh, run_callgrind_all.sh, run_e2e_ab.sh.
- Correctness (tests + bit-exact masks on 23,448 + 21,202 trace steps + 17,167 fuzz steps + 87 scripted steps x 2 vocabs): PASS for p1, p2, skip, eog, pr28371c (#28371 at c29085b), comboc (ours + c29085b). FAIL for #28371 head f000de6 (mask diffs + segfaults; use-after-free confirmed by memcheck, D3.11).
- P1 memory per apply (Qwen): 4.5 MB in 1 allocation vs baseline 11.6 MB in 151,659 allocations.
- Callgrind (instructions, 40 steps/trace): median9 base 60.75G, p1 38.98G, p2 41.09G, skip 40.77G, eog 39.91G; worst10 base 193.12G, p1 168.94G, p2 180.47G, skip 176.99G, eog 179.15G. P2 adds instructions (memory-traffic optimization), so wall time decides.
- Who-benefits audit: results/who_benefits.md (correction: server speculative drafters carry no grammar).
- Running: main timed A/B (base/p1/p2/pr28371c/comboc, 3 interleaved runs) -> results/raw/ab-main. Next: small-ideas A/B (p2/skip/eog), callgrind for PR arms, keep/reject decisions, results/experiments.md.
- Main A/B (fc-v114, 3 runs): P1 1.458x vs base (kept); P2 0.976x vs P1 (rejected); #28371 c29085b 0.963x vs base; head f000de6 fails the gate (use-after-free).
- Small ideas on P1 (2 runs): skip 0.999x, eog 1.002x (rejected); ASCII table 1.036x (kept as 0002); P2b 0.965x (rejected).
- VM restarted mid-run (kernel fc-v114 -> fc-v139). Found a cold-start page-fault regression (fresh process, stack-heavy grammar: 0001+0002 up to 1.6x slower than base), confirmed via GLIBC_TUNABLES; fixed by 0003 (reuse reject lists). Cold A/B: 1.381x, 19/19 faster.

## Phase 4: validate - DONE
- Final vs base, 5 interleaved runs (fc-v139): 1.470x [1.455, 1.486]; median 27.3 -> 18.5 ms; p99 159 -> 127 ms; 206/206 faster. TinyLlama 1.258x.
- E2E (28 grammars x 2 rounds): default path 34.89 -> 35.42 tok/s (+1.5%); grammar_first 15.73 -> 19.54 tok/s (+24.2%); identical tokens.
- llama-bench: no difference beyond noise. Tests 4/4; masks bit-identical on 44,650 steps; fuzz identical.
- Callgrind (results/profile/instructions.md): final vs base -47.1% (median9), -38.7% (worst10).

## Phase 5: write-up - DONE
- README.md, results/REPORT.md, results/experiments.md, results/who_benefits.md, LEARNING.md (parts 1-3), UPSTREAM.md, RESUME.md, LICENSE (MIT), third-party credits in README.

## Final polish (2026-10-17) - DONE
- VM restarted again, now on a different host CPU ("Xeon @ 2.10GHz", model 207; the study ran on "@ 2.80GHz").
- `scripts/quick_verify.sh` (+ `data/quick_subset.txt`, `run_bench.sh -s/-o`, `build_variant.sh` TARGETS, ccache optional): 19 min 25 s from scratch without ccache; masks bit-identical on 23,448 + 21,202 steps; subset A/B 1.545x [1.490, 1.606] on the new CPU (~1.43x for the same subset on the study host). D3.18.
- Bugs found by it: `check_masks.sh` shard glob replayed the first build's output files (explains D3.8's rc=1; verdicts unaffected); `apply_patches.sh` now prunes stale worktrees.
- `results/baseline.md`: note that its 31.3 ms is the pre-reboot (fc-v114) baseline and the 27.3 ms headline is `ab-final`.
- Cross-check of every number in README/REPORT/RESUME/LEARNING (workflow, 528 claims): 15 confirmed findings + 2 from the verifiers; 13 fixed, 4 kept and labelled as single runs; D3.19. A second review of the polish diff itself (205 claims) found 17 more, all fixed. New evidence committed: `results/cold-faults/` (single-run page faults, re-run), `results/pr28371/` (memcheck logs), `results/quick-verify/`.
- `UPSTREAM.md` section 4a: CONTRIBUTING.md steps not done (full `ci/` run, non-glibc platform, perplexity, other compilers/CPUs).

## Independent quick-verify on a laptop (2026-10-26) - DONE
- `scripts/quick_verify.sh` with the v2 subset (4d4a21d) on an Intel Core Ultra 5 125H, Debian 12 under WSL2 (18 logical CPUs, builds -j4, no ccache, on AC power): 17 min 29 s from scratch; masks bit-identical on 23,448 + 21,202 steps and on the 1,259 subset steps; subset A/B 1.590x [1.556, 1.626], 20/20 faster (~1.49x expected from `ab-final`).
- Python 3.11 there, so numpy 2.4.6 instead of the pinned 2.5.4. Results and environment: `results/quick-verify/v2-core-ultra-5-125h-wsl2/`; new index `results/quick-verify/README.md` (the path `quick_verify.sh` already cites).
- Write-up pass (cloud thread): all three quick_verify runs are 7-9% above `ab-final` for their subset, including one on a host with the study's CPU name, so REPORT section 7 no longer says the CPU explains it (cause not established). On the laptop 16 of 20 grammars are within -4..+9% of `ab-final`; o46322 is +40% (1.735x), o17072 / wp_95 / wp_120 about +20%. Per-grammar run-to-run spread is close to the VM runs'. `results/quick-verify/README.md` expanded (subset rules, per-run table, findings); README, UPSTREAM 4a, D3.18 updated.
