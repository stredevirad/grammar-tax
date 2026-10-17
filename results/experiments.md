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
| P2 first-char pre-filter per stack | `patches/rejected/0002-grammar-first-char-prefilter-on-0001.patch` | yes | **0.976x vs P1** [0.967, 0.986] (slower); median +2.2%, p99 -0.6% [CI -15% .. +20%], total +4.0%; 141 of 206 grammars slower | +5.4% / +6.8% vs P1 | **rejected** |
| #28371 at head f000de6 (third arm) | `patches/third_party/pr28371-f000de6.diff` | **no** (mask diffs, segfaults; use-after-free, D3.11) | not timed | - | excluded |
| #28371 at c29085b (cache cleared per call) | `patches/third_party/pr28371-c29085b.diff` | yes | **0.963x vs base** [0.953, 0.973] (slower); median +1.4%, total +2.2% | +0.1% / +0.1% vs base | not ours; reported |
| ours (P1+P2) + #28371 c29085b | `patches/third_party/pr28371-c29085b-on-0002.diff` | yes | **0.975x vs P1+P2** [0.967, 0.982] (slower) | +0.2% / +0.0% vs 0001+P2 | does not combine usefully on this workload |
| reuse reject lists across stacks (fixes the cold-start regression) | `patches/0003-grammar-reuse-reject-lists.patch` | yes | cold (fresh process per grammar, 19 grammars): **1.381x vs base** [1.336, 1.426], 19/19 faster (0001+0002 alone: 3/19 slower, down to 0.625x); warm: **0.982x vs 0001+0002** [0.978, 0.987] | -0.0% / -0.1% vs 0001+0002 | **kept** (removes a regression of up to +60% in the cold case; costs ~2% warm) |
| skip advance when no candidate survives | `patches/rejected/0002-grammar-skip-advance-no-survivors.patch` | yes | **0.999x vs P1** [0.990, 1.008]; median +0.3%, total +1.3% (2 runs, fc-v114) | -0.9% / -2.1% vs 0001 | **rejected** (no measurable gain) |
| cheaper is_eog (scan of eog ids) | `patches/rejected/0002-grammar-eog-scan.patch` | yes | **1.002x vs P1** [0.993, 1.012]; median +0.9%, total +1.1% (2 runs, fc-v114) | -3.1% / -0.8% vs 0001 | **rejected** (no measurable gain; also adds a vocab accessor) |
| ASCII match table in `reject_candidates_for_stack` | `patches/0002-grammar-ascii-match-table.patch` | yes | **1.036x vs P1** [1.028, 1.046] (2 runs, fc-v114); **1.033x vs P1** [1.022, 1.045], median -4.1%, p99 -7.0%, total -4.2% (3 runs, fc-v139) | -17.5% / -29.9% vs 0001 | **kept** (consistently just above the 3% bar, +12 / -2 lines) |
| P2b: compact first-code-point scan per stack | `patches/rejected/0002-grammar-first-char-scan.patch` | yes | **0.965x vs P1** (fc-v114, 2 runs); **0.950x vs P1** [0.940, 0.961], median +6.3%, p99 -7.3% [CI -21% .. +6%] (fc-v139, 3 runs) | -0.9% / -15.4% vs 0001 | **rejected** (slower on typical grammars; only fixes the cold-start case, D3.13) |

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
  (memcheck: 8,829 errors, invalid reads and uninitialised values, in the memo cache's hash lookup; baseline 0) (D3.11).
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

## Re-measurement on the new kernel (fc-v139, `results/raw/ab-v139`, 3 interleaved runs)

After the VM restart (D3.12) the remaining candidates were re-measured together with the baseline:

| vs base | per-grammar speedup | median per-token | pooled p99 | total |
|---|---|---|---|---|
| P1 | 1.387x [1.368, 1.407] | -27.9% | -11.8% | -23.7% |
| P1 + ASCII table (final series) | **1.433x** [1.415, 1.453] | **-32.5%** | **-24.3%** | **-27.0%** |
| P2b | 1.318x [1.300, 1.337] | -23.3% | -25.4% | -22.8% |

P1's numbers here are a little lower than on fc-v114 (1.458x); the two runs are on different kernels and
days, so only within-run comparisons are used for decisions.

## ASCII match table (kept as 0002)

- What: in `llama_grammar_reject_candidates_for_stack`, for candidate lists of at least 128 entries,
  compute `llama_grammar_match_char(stack_pos, c)` once for each ASCII `c` and look it up per candidate.
  The result and the order of the output are unchanged (same function, same inputs).
- Measured vs P1 in two separate A/Bs on two kernels: +3.6% and +3.3% per grammar (geomean), total time
  -3.1% and -4.2%; on the post-reboot run also median -4.1% and pooled p99 -7.0%.
- Why it helps: after P1, `llama_grammar_match_char` is ~18% of time on the worst set; for an ASCII first
  char (most of the vocab) the lookup replaces a walk over the char class.

## P2b: compact first-code-point scan (rejected)

- What: P2's per-stack pre-filter, but scanning two compact arrays (first code point 4 B + accepted flag
  1 B per candidate, with an ASCII table) instead of the 32-byte candidate structs.
- Result: slower than P1 on the typical grammar (median +6.3%) and not significantly better on p99.
  The per-stack scan is cheap, but it still touches every candidate for every stack, and for the many
  candidates that pass the first char (e.g. tokens starting with a space at a whitespace position) the
  full check runs as before.
- Its one real advantage is structural: it never makes the two ~4.8 MB allocations per stack, so it avoids
  the cold-start page-fault mode (D3.13). That case is quantified below; it was not judged worth a 5%
  loss on typical grammars plus 52 lines.

## 0003: reuse the reject lists across stacks (kept)

- Found after the VM restart: replaying the 600-stack schema alone in a fresh process, 0001+0002 was 60% slower than
  baseline; the cause is glibc mmap/trim behavior on the ~4.8 MB reject list allocated per stack in the pre-existing
  code, which the baseline's small allocations had masked (DECISIONS D3.13, D3.15).
- Cold A/B (`results/raw/cold-final` vs `results/raw/cold-reuse`, 19 grammars, each alone in a fresh process, 2 rounds):
  0001+0002 vs base 1.250x but o46322 0.625x, rustfmt 0.756x, sp_408 0.806x; 0001+0002+0003 vs base **1.381x**
  [1.336, 1.426], all 19 faster (slowest 1.223x).
- End to end, fresh `grammar_record` process with the real model, full mask every step: o46322 grammar time base 55.1 /
  54.5 / 55.1 s, 0001+0002 81.0 / 82.7 / 81.6 s, final 59.3 / 55.5 / 54.8 s (page faults base 3.6M, 0001+0002 21.9M,
  final 9.2M). So the regression is mostly but not entirely gone for this one schema (-0.6% to +7.7% vs base over three
  runs); rustfmt base 21.1 / 21.6 s vs final 23.0 / 21.5 s; sp_408 final 17-27% faster. Removing the rest would mean
  passing scratch buffers down the recursion (`next_candidates`), judged too much code for the remaining few percent.
- Warm (5 runs, `results/raw/ab-final`): 0.982x [0.978, 0.987] vs 0001+0002, i.e. ~2% slower in the warm process.
  Kept anyway: one generation per process (`llama-cli`, `llama-completion`) is the cold case.

## Final validation (Phase 4, fc-v139)

| | result |
|---|---|
| final vs base, 5 interleaved runs, 206 grammars (Qwen vocab) | **1.470x** [1.455, 1.486] per grammar; median -32.7% [-34.0, -31.2]; pooled p99 -17.9% [-24.5, -14.8]; total -28.8%; 206/206 faster |
| 0001+0002 vs base in the same session | 1.497x [1.480, 1.514]; median -32.4%; p99 -22.8% |
| final vs base, TinyLlama vocab, 3 runs | 1.258x [1.223, 1.298] per grammar; median -5.9% [-12.7, -1.4]; p99 +1.6% (n.s.); total -3.4% |
| end to end (28 grammars, 2 interleaved rounds) | default path +1.5% pooled tok/s (+3.1% per grammar, range over rounds 1.2 .. 5.0); full mask every step +24.2% (+23.9%, 22.3 .. 25.6); identical token sequences |
| llama-bench, no grammar | pp512 282.1 / 264.2 vs 279.4 / 280.8 t/s; tg128 43.4 / 43.1 vs 42.1 / 42.8 t/s; per-test stddev 13-19 (pp) and 1.0-2.4 (tg) t/s: no difference beyond noise (the grammar code is not executed) |
| tests, masks, fuzz | 4/4 tests pass; masks bit-identical on 23,448 + 21,202 steps; fuzz 13,499 + 3,668 steps and 87 x 2 scripted steps identical |
