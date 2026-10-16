# Experiments: kept and rejected ideas

Every idea is a patch on the pinned SHA, measured the same way:

- **Correctness gate** (all must pass before timing counts): the 4 llama.cpp grammar tests; bit-exact mask
  hashes vs baseline on every step of all 206 traces with both vocabs (23,448 Qwen + 21,202 TinyLlama
  steps, `scripts/check_masks.sh`); random-walk differential test over all `grammars/*.gbnf` plus 7
  edge-case grammars with both vocabs (13,499 + 3,668 steps) and 87 scripted edge-case steps per vocab
  (`scripts/run_fuzz.sh`).
- **Wall time**: interleaved runs of the full 206-grammar replay workload (Qwen 151,936-token vocab, full
  mask every step), run order alternating so that run *i* of every build ran back to back; paired
  per-grammar ratios with bootstrap 95% CIs over grammars (`scripts/ab_stats.py`).
- **Instructions**: callgrind on the median-9 and worst-10 grammar sets, first 40 tokens per trace
  (deterministic, independent of VM noise).
- **Keep rule** (from the brief): correct and at least ~3% faster; otherwise rejected and logged here.

## Summary

| idea | patch | correct | wall time vs its base (geomean per-grammar speedup, 95% CI) | instructions (median9 / worst10) | decision |
|---|---|---|---|---|---|
| P1 flat code-point buffer | `patches/0001-grammar-flat-decode-buffer.patch` | yes | **1.458x vs base** [1.441, 1.477]; median cost -31.6%, pooled p99 -21.5%, total -27.9% | -35.8% / -12.5% vs base | **kept** |
| P2 first-char pre-filter per stack | `patches/rejected/0002-grammar-first-char-prefilter.patch` | yes | **0.976x vs P1** [0.967, 0.986] (slower); median +2.2%, p99 -0.6% [CI -15% .. +20%], total +4.0%; 141 of 206 grammars slower | +5.4% / +6.8% vs P1 | **rejected** |
| #28371 at head f000de6 (third arm) | `patches/third_party/pr28371-f000de6.diff` | **no** (mask diffs, segfaults; use-after-free, D3.11) | not timed | - | excluded |
| #28371 at c29085b (cache cleared per call) | `patches/third_party/pr28371-c29085b.diff` | yes | **0.963x vs base** [0.953, 0.973] (slower); median +1.4%, total +2.2% | (pending) | not ours; reported |
| ours (P1+P2) + #28371 c29085b | `patches/third_party/pr28371-c29085b-on-0002.diff` | yes | **0.975x vs P1+P2** [0.967, 0.982] (slower) | (pending) | does not combine usefully on this workload |
| skip advance when no candidate survives | `patches/candidates/0002-grammar-skip-advance-no-survivors.patch` | yes | **0.999x vs P1** [0.990, 1.008]; median +0.3%, total +1.3% (2 runs, fc-v114) | (on P2: -0.8% / -1.9%) | **rejected** (no measurable gain) |
| cheaper is_eog (scan of eog ids) | `patches/candidates/0002-grammar-eog-scan.patch` | yes | **1.002x vs P1** [0.993, 1.012]; median +0.9%, total +1.1% (2 runs, fc-v114) | (on P2: -2.9% / -0.7%) | **rejected** (no measurable gain; also adds a vocab accessor) |
| ASCII match table in `reject_candidates_for_stack` | `patches/candidates/0002-grammar-ascii-match-table.patch` | yes | **1.036x vs P1** [1.028, 1.046]; median -2.5%, total -3.1% (2 runs, fc-v114) | (pending) | borderline; re-measured on fc-v139 (D3.13) |
| P2b: compact first-code-point scan per stack | `patches/candidates/0002-grammar-first-char-scan.patch` | yes | **0.965x vs P1** [0.955, 0.974] on fc-v114 (2 runs); on fc-v139 it avoids P1's page-fault slow mode on stack-heavy grammars (D3.13) | (pending) | re-measured on fc-v139 |

Raw data: `results/raw/ab-main/` (base, p1, p2, pr28371c, comboc; 3 interleaved runs), statistics:
`results/ab-main/ab_<a>_vs_<b>.md`, instruction counts: `results/profile/instructions.tsv`.

## P1: flat code-point buffer (kept)

- What: decode all candidate pieces into one buffer per `llama_grammar_apply_impl` call instead of one
  `std::vector<uint32_t>` per token (LEARNING.md Part 2).
- Wall time (3 interleaved runs, 206 grammars): per-grammar speedup 1.458x [1.441, 1.477]; median per-token
  cost 0.684x of base [0.668, 0.701]; pooled p99 0.785x [0.722, 0.854]; total replay 0.721x [0.702, 0.735].
- Instructions: median9 60.75G -> 38.98G (-35.8%); worst10 193.12G -> 168.94G (-12.5%).
- Memory: per apply call (Qwen vocab) 4.5 MB in one allocation instead of 11.6 MB in 151,659 allocations
  (TinyLlama: 0.83 MB vs 2.3 MB in 31,999). Peak RSS of the replay process (vocab + workload): base 142 MB,
  P1 133 MB (run 1).
- Why it matched the prediction: the profile put malloc/free at 25.6% of time on the median set; the
  measured median gain (31.6%) is a bit larger because contiguous code points also speed up every later
  pass that reads them.

## P2: per-stack first-character pre-filter (rejected)

- Hypothesis (profile.md): the per-stack sweep on stack-heavy grammars is memory-bound because each stack
  copies almost the whole candidate list into a new reject list; skipping candidates whose first code point
  cannot match, and marking acceptance instead of copying rejects, should cut p99 by 40-60%.
- Result: slower. Paired vs P1: 0.976x [0.967, 0.986] per grammar, median cost +2.2%, total +4.0%; the
  pooled p99, the target, did not move (-0.6%, CI -15% .. +20%). Instructions went up on both sets
  (median9 +5.4%, worst10 +6.8% vs P1).
- Why the hypothesis failed (checked against profiles, not assumed): P2 assumed the cost of a stack's sweep
  was the *writes* (copying ~152k rejected candidates into a new list). The profiles of P1 and P2 say the
  *reads* set the cost:
  - callgrind worst10: P2 moved the per-candidate loop from `llama_grammar_reject_candidates_for_stack`
    (47.0G instructions in P1) into `llama_grammar_apply_impl` (45.0G in P2) and added
    `llama_grammar_match_char` calls (77.4G -> 83.9G, candidates that pass are tested twice);
  - perf worst10 after P1: `llama_grammar_reject_candidates_for_stack` is still 63.5% of time but ~37% of
    instructions (`results/profile/worst10-p1-perf/`), so after P1 the sweep is still memory-bound;
  - both versions read, for every stack, the full 152k x 32-byte candidate array (4.8 MB) plus one code point
    per token from the 4.5 MB code-point buffer: ~9 MB of reads per stack, which at the measured ~2.3 ms per
    stack is ~4 GB/s. P2 removed the sequential writes (cheap) and kept these reads (expensive).
- Follow-up (P2b below): the same idea with a compact per-step array of first code points (4 bytes per
  candidate) and the 1-byte accepted flags, so each stack scans ~0.75 MB instead of ~9 MB.
- Kept as `patches/rejected/` with its correctness evidence; LEARNING.md keeps its walkthrough because the
  correctness argument (per-candidate independence) is reused by P2b.

## #28371 (open upstream PR, memoization) as a third arm

- Applies cleanly to the pinned SHA (D3.9). At its head f000de6 it fails the correctness gate: mask
  differences and crashes, caused by cache keys that keep raw pointers into per-call buffers across steps
  (memcheck: 8,829 invalid reads; baseline 0) (D3.11).
- Its previous commit c29085b clears the cache every call and passes every gate. On this workload it is
  slightly slower than baseline (0.963x per grammar, total +2.2%): the memo only engages for candidate
  lists of at most 8 entries, which are rare here, and building content-hashed keys costs time. Combined with
  our patches it is also slightly slower (0.975x vs P1+P2).
- Scope of this result: the PR's own benchmark targets a degenerate, highly ambiguous grammar (where the
  baseline timed out). Our workload (built-in grammars + JSONSchemaBench) contains no such grammar, so this
  says nothing against the PR on that case; it says the PR does not speed up typical JSON-schema masks.

## Small ideas on top of P1 (fc-v114 kernel, `results/raw/ab-small`, runs 1-2)

The user-requested small ideas were first built on top of P1+P2; after P2 was rejected they were rebuilt
on top of P1 alone and timed there (instruction counts on top of P2 are kept in
`results/profile/instructions.tsv` for the record).

- **Skip advance/recursion when no candidate matched the first char** (3 lines): 0.999x [0.990, 1.008].
  Rejected: `llama_grammar_advance_stack` was under 1% of the time even on the worst grammars (profile.md),
  so skipping some calls cannot reach 3%. Instructions on top of P2: -0.8% (median9), -1.9% (worst10).
- **Cheaper is_eog** (8 lines, adds `llama_vocab::get_eog_ids`): 1.002x [0.993, 1.012]. Rejected: the
  per-token `std::set` lookup was 1.7% of time at baseline; replacing it with an inline scan of 6 ids
  saved ~3% of instructions on top of P2 (median9) but nothing measurable in wall time, and it widens the
  change to the vocab interface.
- The third run of this A/B was lost to a VM restart (D3.12); both decisions are far from the 3% bar, so two
  paired runs over 206 grammars are enough (CI width about +-1%).
