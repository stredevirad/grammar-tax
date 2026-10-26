# Report: faster GBNF grammar masks in llama.cpp on the CPU

A record/replay study of llama.cpp's built-in grammar engine (pinned at `620d2225`, 2026-10-15), three small
patches that make the full-vocab mask about 1.47x faster on a 206-grammar workload with bit-identical masks,
and everything that did not work.

**What the headline numbers measure.** They are for **mask computation**: one call to
`llama_grammar_apply_impl` over the whole vocab plus the token accept, replayed from recorded generations
with no model in the loop. On llama.cpp's default sampling path the full mask is only needed when the sampled
token is rejected, so the **end-to-end throughput gain there is about 2%** (measured: +1.5% pooled tokens/s).
The gain is large (+24% tokens/s measured) only where the full mask runs every step: `grammar_first = true`
(e.g. the `llama-speculative` example) and applications that put the grammar sampler in a `llama_sampler`
chain. See [who_benefits.md](who_benefits.md) for every such path with file:line.

## 1. Results at a glance

Final series: [`0001`](../patches/0001-grammar-flat-decode-buffer.patch) (decode all pieces into one buffer),
[`0002`](../patches/0002-grammar-ascii-match-table.patch) (ASCII match table),
[`0003`](../patches/0003-grammar-reuse-reject-lists.patch) (reuse the reject lists across stacks).
+34 / -14, +12 / -2 and +26 / -7 lines (git numstat), all in `src/llama-grammar.cpp`.

| measurement | baseline | final | change | runs |
|---|---|---|---|---|
| **per-grammar speedup** (geomean of paired per-grammar ratios), Qwen2.5 vocab 151,936 | | | **1.470x** [1.455, 1.486] | 5 interleaved, 206 grammars |
| median per-token mask cost (median over grammars of each grammar's mean) | 27.3 ms [25.4 .. 28.8] | 18.5 ms [18.1 .. 19.0] | **-32.7%** [-34.0, -31.2] | 5 |
| pooled p99 per-token cost (all 23,448 tokens) | 159 ms [140 .. 168] | 127 ms [123 .. 142] | **-17.9%** [-24.5, -14.8] | 5 |
| total replay time of the workload | 772 s [744 .. 814] | 552 s [543 .. 579] | -28.8% | 5 |
| grammars faster / slower | | | 206 / 0 (range 1.03x .. 1.89x) | 5 |
| cold start (each of 19 grammars alone in a fresh process) | | | 1.381x [1.336, 1.426]; 19 / 0 faster | 2 rounds |
| TinyLlama vocab (32,000 tokens), separate traces | 4.8 ms median | 4.5 ms | 1.258x [1.223, 1.298] per grammar; median -5.9%, p99 +1.6% (n.s.) | 3 |
| **end to end, default sampling path** (28 grammars, real model) | 34.89 tok/s | 35.42 tok/s | **+1.5%** pooled; +3.1% per grammar [1.2 .. 5.0 over rounds] | 2 interleaved rounds |
| **end to end, full mask every step** (`grammar_first`) | 15.73 tok/s | 19.54 tok/s | **+24.2%** pooled; +23.9% per grammar [22.3 .. 25.6 over rounds] | 2 |
| no grammar (`llama-bench`, pp512 / tg128) | 282 / 264 t/s; 43.4 / 43.1 t/s | 279 / 281 t/s; 42.1 / 42.8 t/s | within noise (per-test stddev 1-2.4 t/s on tg128); grammar code is not executed | 2 x 5 reps |
| transient memory per apply call (Qwen vocab) | 11.6 MB in 151,659 allocations | 4.5 MB in 1 allocation (+ the reject lists) | -61% | exact (computed) |
| peak RSS of the replay process | 140-142 MiB | 132-133 MiB | -5% | 5 |
| **correctness** | | | masks bit-identical on every step: 23,448 (Qwen) + 21,202 (TinyLlama) trace steps, 17,167 random-walk steps, 87 x 2 scripted edge-case steps; 4/4 grammar tests pass | |

**Instruction counts** (callgrind, deterministic; first 40 tokens per trace; [instructions.md](profile/instructions.md)):
final vs baseline **-47.1%** on the median-9 grammars (60.75G -> 32.13G) and **-38.7%** on the worst-10
(193.12G -> 118.35G). Full table, including rejected arms, in section 5.

All A/B numbers are paired and interleaved (run *i* of each build back to back, order alternating), with
95% bootstrap CIs over grammars from [`scripts/ab_stats.py`](../scripts/ab_stats.py) (2,000 resamples for
`ab-final` and `ab-final-tinyllama`, 1,000 for the cold A/Bs, `ab-main` and `ab-v139`, 500 for `ab-small`).
The end-to-end brackets are min .. max over the 2 rounds, not CIs.
Raw data: `results/raw/ab-final/` (Qwen), `results/raw/ab-final-tinyllama/`, `results/raw/cold-reuse/`,
`results/e2e/`. Kernel for these runs: `6.18.44-fc-v139` (see section 7).

![per-token cost CDF](ab-final/charts/cdf_per_token.png)
![per-grammar speedup](ab-final/charts/speedup_reuse.png)

## 2. Method

**Workload** ([baseline.md](baseline.md)): all 8 `grammars/*.gbnf` plus 198 JSONSchemaBench schemas (test
split, 20 per subset, seed 42), converted with llama.cpp's own `json_schema_to_grammar`. One constrained
generation per grammar was recorded with Qwen2.5-0.5B-Instruct (and separately with TinyLlama-1.1B-Chat):
23,448 tokens, 161 generations ending in EOG and 45 hitting the 256-token cap.

**Replay** ([`bench/grammar_replay.cpp`](../bench/grammar_replay.cpp)): loads only the vocab, builds the
grammar and, for every recorded token, times `llama_grammar_apply_impl` over the full vocab and
`llama_grammar_accept_impl`. `--check` hashes the reject bit of every token at every step.

**Profiling** ([profile.md](profile.md)): callgrind instruction counts (deterministic; on a
`GGML_NATIVE=OFF` build because valgrind cannot decode AVX-512; libllama is compiled identically in both)
and perf cpu-clock sampling (no hardware counters in this VM).

**Noise.** The VM drifted ~20% over an hour (baseline runs 1050 s -> 832 s). Hence: every comparison
interleaves builds within one session, pairs run *i* with run *i*, and reports per-grammar ratios with
bootstrap CIs; nothing else ran during timed runs; instruction counts back up timing.

**Keep rule** (from the brief): a patch is kept only if it passes every correctness gate and is at least ~3%
faster.

## 3. Correctness

Every patch and every arm went through the same gates (`scripts/check_masks.sh`, `scripts/run_fuzz.sh`,
`scripts/run_tests.sh`):

1. the 4 llama.cpp grammar tests (`test-grammar-parser`, `test-grammar-integration`, `test-llama-grammar`,
   `test-json-schema-to-grammar`);
2. bit-identical mask hashes vs baseline on every step of all 206 traces, both vocabs (44,650 steps);
3. a differential random-walk test ([`bench/grammar_fuzz.cpp`](../bench/grammar_fuzz.cpp)): seeded random
   valid walks through all 8 built-in grammars (including `japanese.gbnf`) and 7 edge-case grammars
   ([`bench/fuzz_grammars/`](../bench/fuzz_grammars/)) with both vocabs, biased toward tokens that end inside a
   UTF-8 sequence and toward non-ASCII and byte-fallback tokens. Identical masks give identical walks, so
   the outputs are compared line by line (17,167 steps). Coverage counters confirm the walks hit partial
   UTF-8 carried into a step, single-byte fallback tokens, multibyte tokens, EOG allowed and taken, empty
   stacks, ranges, alternates, negated classes, any-char, TOKEN and TOKEN_NOT tops;
4. scripted edge cases ([`cases.tsv`](../bench/fuzz_grammars/cases.tsv)): partial UTF-8 across 2, 3 and 4
   tokens (kana, emoji, Latin-1), negated classes with a pending partial, an invalid continuation, empty stacks
   with EOG, token elements, Japanese built from byte tokens; 87 steps per vocab, compared exactly.

Results: every kept and rejected patch of ours passed all four. Upstream PR #28371 at its head did not
(section 6).

## 4. The kept patches

Full line-by-line walkthrough: [LEARNING.md](../LEARNING.md) Part 2.

### 0001: decode all candidate pieces into one buffer

- **Problem:** `llama_grammar_apply_impl` decoded each of the ~152k vocab pieces into its own
  `std::vector<uint32_t>` on every call: 151,659 heap allocations and frees per step, ~35% of instructions and
  ~26% of time on the median grammars.
- **Change:** an appending variant of `decode_utf8`; one buffer per call reserved to an exact upper bound (each
  code point consumes at least one byte; plus one terminator per piece); candidates point into it; an assert
  checks the buffer never moved. The per-piece `decode_utf8` used by accept becomes a wrapper.
- **Measured** (fc-v114, 3 runs): 1.458x [1.441, 1.477] per grammar vs baseline, median -31.6%, p99 -21.5%.
- **Memory:** per call 4.5 MB in one allocation instead of 11.6 MB in 151,659 allocations; nothing is kept
  between calls.
- **How it differs from #6811 (closed 2024):** #6811 cached decoded code points across steps
  (vocab-sized persistent memory, lazy init with a mutex) and was closed because, after the resampling fix
  (#7424), it showed no end-to-end gain. 0001 has no persistent state, no locking, and uses *less* memory
  than the baseline. The end-to-end caveat that sank #6811 still applies to the default path, and this report
  states it (about 2%).

### 0002: precompute the char match for ASCII in long candidate lists

- **Change:** in `llama_grammar_reject_candidates_for_stack`, for lists of >= 128 candidates, call
  `llama_grammar_match_char(stack_pos, c)` once for each ASCII `c` and look it up per candidate. Pure function,
  same results, same output order.
- **Measured vs 0001:** +3.6% [2.8, 4.6] (fc-v114, 2 runs) and +3.3% [2.2, 4.5] per grammar (fc-v139, 3
  runs), total time -3.1% and -4.2%. Just above the 3% bar, consistently, for 12 added lines.

### 0003: reuse the reject lists across stacks

- **Problem, found after the first two patches:** replaying a 600-stack schema alone in a fresh process,
  0001+0002 was ~60% *slower* than baseline (375 vs 235 ms/token in the cold A/B); single `perf stat` runs of
  the same case counted 20.9M page faults vs 3.4M ([cold-faults/README.md](cold-faults/README.md)); end
  to end with the real model, its grammar time went from 53 s to 81 s. Cause: the pre-existing code allocates a
  new reject list per stack (~4.8 MB at the top level) and fills it; while glibc's dynamic mmap threshold is
  still low, each one is a fresh mmap that the kernel faults in page by page. The baseline's 152k small live
  allocations had been masking this; 0001 removed them. Confirmed by `GLIBC_TUNABLES` (raised mmap/trim
  thresholds): faults drop to 18k and 0001+0002 becomes 1.31x faster than baseline on the same cold run.
  Running the grammar after another one in the same process also removes it, which is why the warm A/B runs had
  not shown it (section 7).
- **Change:** an internal `llama_grammar_reject_candidates_for_stack_into` writes into a caller-owned list;
  `llama_grammar_reject_candidates` alternates two lists across the stacks. The public
  `llama_grammar_reject_candidates_for_stack` (checked for output order by `test-llama-grammar`) is a wrapper
  with the same output.
- **Measured:** cold 600-stack replay 377.6 -> 180.3 ms/token, faults 20.9M -> 0.6M (single `perf stat` runs,
  same session; in the paired cold A/B, o46322 base 258.2 vs final 194.0 ms/token); cold A/B on 19 grammars
  1.381x [1.336, 1.426], all 19 faster (0001+0002 alone had 3 of 19 slower, down to 0.625x). End to end cold,
  o46322 grammar time base 55.1 / 54.5 / 55.1 s vs final 59.3 / 55.5 / 54.8 s (three runs: +7.7%, +1.9%,
  -0.6%), page faults 3.6M vs 9.2M; rustfmt 21.1 / 21.6 vs 23.0 / 21.5 s; sp_408 -17% / -27%. **Cost in the warm
  case:** 0.982x [0.978, 0.987] vs 0001+0002 (about 2% slower), kept because it removes a large regression in
  the one-generation-per-process case (`llama-cli`, `llama-completion`).

## 5. What did not work (and why)

Details and numbers: [experiments.md](experiments.md).

| idea | result | why |
|---|---|---|
| **P2:** per-stack first-char pre-filter (the Phase 2 recommendation for p99) | correct; **0.976x vs 0001** (2.4% slower), p99 unchanged | designed against the baseline profile; after 0001 the per-stack cost was reading the candidate list, not writing reject copies, and P2 added a second match per surviving candidate |
| **P2b:** P2 over compact first-code-point arrays | correct; 0.965x / 0.950x vs 0001 (two kernels) | the scan was not the bottleneck either; many candidates pass the first char (space-prefixed tokens at whitespace positions) and still need the full check. Its only advantage (no per-stack lists) was later obtained by 0003 |
| skip advance/recursion when nothing matched | correct; 0.999x [0.990, 1.008] vs 0001 | `advance_stack` (self) was under 1% of time even at 600 stacks |
| cheaper `is_eog` (scan of eog ids) | correct; 1.002x [0.993, 1.012] vs 0001 | the `std::set` lookup was 1.7% of time; also needed a new vocab accessor |
| cheaper stack de-dup (recover #18604's ~13%) | not implemented | profile: `advance_stack` is 0.7% of instructions self and 2.4% including the `std::set` de-dup and its allocations, over the worst-10 grammars (first 40 steps each, up to 600 stacks) |
| persistent code-point cache (#6811 idea) | not implemented | memory, locking, and the documented lack of end-to-end gain |

Instruction counts for every arm (callgrind, same method as section 1):

| variant | median9 instructions | vs base | worst10 instructions | vs base |
|---|---|---|---|---|
| baseline | 60.75 G | +0.0% | 193.12 G | +0.0% |
| 0001 | 38.98 G | -35.8% | 168.94 G | -12.5% |
| 0001+0002 | 32.14 G | -47.1% | 118.42 G | -38.7% |
| final 0001+0002+0003 | 32.13 G | -47.1% | 118.35 G | -38.7% |
| 0001+P2 (rej.) | 41.09 G | -32.4% | 180.48 G | -6.5% |
| 0001+P2b (rej.) | 38.63 G | -36.4% | 142.92 G | -26.0% |
| 0001+skip (rej.) | 38.65 G | -36.4% | 165.44 G | -14.3% |
| 0001+eog (rej.) | 37.79 G | -37.8% | 167.62 G | -13.2% |
| #28371 c29085b | 60.84 G | +0.1% | 193.25 G | +0.1% |
| 0001+P2+#28371 | 41.17 G | -32.2% | 180.56 G | -6.5% |

P2 and the #28371 arms do not reduce instructions; P2b and the two small ideas reduce them a little but not
wall time (P2b: -15% instructions vs 0001 on the worst set, yet 3.5-5% slower), which says the remaining
per-stack cost is memory latency, not instruction count.

**A misleading smoke test** (kept as a lesson): a single replay of the worst grammar on a loaded machine showed
P2b halving P1's time. Interleaved A/B on a quiet machine showed no such gain; the smoke test had measured P1's
cold/contended slow mode, not P2b's scan. Only interleaved A/B runs are used for decisions.

## 6. Upstream PR #28371 as a third arm

- Fetched read-only (`pull/28371/head`, `f000de6`), applied cleanly to the pinned SHA within the 15-minute budget.
- **At its head it fails the correctness gate:** masks differ from baseline (first at `gbnf__english`, step 56)
  and it segfaults on some grammars. valgrind memcheck: 8,829 errors from 30 contexts (invalid reads and uses of
  uninitialised values), all in the memo cache's hashing and lookup (`llama_grammar_candidates_hash`, the hash
  table's find/rehash); baseline: 0 ([pr28371/](pr28371/)). The memo cache's keys copy the candidate vectors, whose `code_points` are raw pointers into
  per-call buffers that are freed after each step; since `f000de6` the cache persists across steps. Nothing was
  posted upstream; see [UPSTREAM.md](../UPSTREAM.md).
- Its previous commit `c29085b` clears the cache every call and passes all gates. On this workload it is
  **0.963x** [0.953, 0.973] vs baseline (slightly slower), and combined with our 0001+P2 it is 0.975x vs ours
  alone: its memo only engages for candidate lists of <= 8 entries, which are rare in full-vocab masks of JSON
  schemas, and building content-hashed keys costs time. Its own benchmark is a degenerate ambiguous grammar
  that this workload does not contain, so this is not a verdict on that case.

## 7. Environment notes and threats to validity

- **VM restart mid-study.** During the small-ideas A/B the VM restarted and the guest kernel string changed
  (`6.18.44-fc-v114` -> `fc-v139`). All decisions use within-session paired comparisons; the final A/B and all
  Phase 4 runs are on fc-v139. 0001 measured 1.458x vs baseline on fc-v114 and 1.387x on fc-v139: absolute
  numbers move between sessions, paired ratios within a session are what is reported.
- **Warm vs cold process.** The replay runs grammars one after another in one process (warm allocator); the
  cold case (section 4, 0003) was found only by replaying a grammar alone. Both are reported.
- **One machine, one allocator.** 4-vCPU x86 VM, glibc malloc. 0003's effect depends on glibc's
  mmap/trim behavior; other allocators (jemalloc, mimalloc) or OSes were not tested.
- **Paired ratios move between sessions and CPUs.** The study's runs record the CPU as "Intel Xeon @ 2.80GHz"
  (`results/raw/*/meta.txt`). `scripts/quick_verify.sh` (3 interleaved runs on a 20-grammar subset) was run
  three times after the study, and masks were bit-identical on all 44,650 steps each time. On its first subset
  (`ab-final`: 1.432x for those 20 grammars) it gave **1.545x** [1.490, 1.606] on a "Xeon @ 2.10GHz" host after a
  second VM restart (2026-10-17; per-token costs 25% (baseline) and 29% (final) lower than in `ab-final`,
  `gbnf__japanese` 1.03x -> 1.34x) and **1.566x** [1.496, 1.633] on a "Xeon @ 2.80GHz" host the same day. On the
  current subset (`ab-final`: 1.488x) it gave **1.590x** [1.556, 1.626] on an Intel Core Ultra 5 125H laptop under
  WSL2 (Debian 12, GCC 12.2, 2026-10-26). All three are 7-9% above `ab-final`, including the run on a host with
  the study's CPU name, so the gap is not (only) the CPU; its cause is not established. The headline 1.470x is
  the 5-run within-session A/B and was not re-run. The worst case (o46322) varies most: 1.24x in `ab-final`,
  1.33-1.36x on the Xeons, 1.735x on the laptop. Results: [quick-verify/](quick-verify/).
- **Workload.** JSON-schema grammars dominate (198 of 206). Highly ambiguous hand-written grammars (the case
  #28371 targets) are not represented.
- **TinyLlama runs were noisier** (some grammars vary +-30% across 3 runs); the per-grammar speedup CI is wide
  and the median/p99 effects are small.
- **Traces from a 0.5B model.** The generated JSON is realistic but simple; real outputs may visit different
  grammar states. The random walks cover states the traces don't.

## 8. Reference points

- **llguidance** (optional backend, built in ~2 min): median 0.41 ms per mask on the 162 schemas it accepts
  (36 rejected by its defaults, 39 diverge on our traces): ~45x faster than the final series' median. Its speed
  comes from a different design (token trie, lexer + Earley parser), not from tuning this loop.
- **Where time still goes** after the patches: the per-stack sweep (every live stack still visits every
  candidate), the per-step UTF-8 decode of the vocab (~14% of time at baseline, untouched), and the recursion
  for candidates that pass the first char. Sharing work across stacks with the same top element is the next idea
  worth trying (LEARNING.md, question 15).

## 9. Reproduce

See [README.md](../README.md#reproduce); `scripts/quick_verify.sh` is the ~20-minute subset of this. The
commands behind this report, with one naming difference: the recorded runs called the final build
`build/bench-reuse` (so the committed data is under `results/raw/ab-final/reuse/` and is analysed with
`--b reuse`), and `ab-final` also ran `build/bench-p1ascii` (0001+0002) as a third arm:

```sh
scripts/apply_patches.sh build/src-final                                 # 0001-0003 on the pinned SHA
scripts/build_variant.sh build/src-final build/bench-final
scripts/run_tests.sh build/bench-final
scripts/check_masks.sh build/bench-base build/bench-final                # bit-exact, both vocabs
scripts/run_fuzz.sh run build/bench-final final && scripts/run_fuzz.sh compare base final
scripts/run_bench.sh -n 5 -l ab-final build/bench-base build/bench-final
build/venv/bin/python -I scripts/ab_stats.py results/raw/ab-final --a base --b final
scripts/run_e2e_ab.sh 2 build/bench-base build/bench-final
```
