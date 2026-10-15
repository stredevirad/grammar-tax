# Profile and GATE

Phase 2 of the study. Inputs: the 5-run baseline (`results/baseline.md`), callgrind instruction
profiles and perf time profiles of `grammar_replay` on two grammar sets. Everything here is about
the **full-vocab mask** (apply); accept is 0.1% of the time.

## 1. Method

| | median set ("median9") | worst set ("worst10") |
|---|---|---|
| grammars | 9 grammars ranked around the workload median of per-grammar mean cost, with >= 40 tokens ([list](baseline-qwen05/median9.txt)): 31.4-32.1 ms/token, 3-16 peak stacks | the 10 highest per-grammar mean costs ([list](baseline-qwen05/worst10.txt)): 54-276 ms/token, 5-600 peak stacks |
| callgrind | first 40 tokens of each trace (D1.13), `--toggle-collect` on apply/accept only: **60.8 G instructions** (360 masks) | first 40 tokens: **193.1 G instructions** (400 masks) |
| perf | full traces, `-e cpu-clock -F 1000 -g`, native build | full traces |

- Callgrind runs on a `GGML_NATIVE=OFF` build because valgrind 3.22 cannot decode AVX-512. libllama
  (all grammar code) is compiled the same way in both builds (`-O3`, no `-march`), D1.7.
- perf has no hardware counters in this VM; cpu-clock sampling gives time shares, callgrind gives
  noise-free instruction shares. **They disagree in an important way** (section 2.3), so both are
  reported.
- Raw outputs: `results/profile/{median9,worst10}/` (`callgrind_excl.txt`, `callgrind_incl.txt`,
  `callgrind_tree.txt`, `perf_report.txt`). An early single-grammar callgrind run on `json.gbnf`
  (18.6 G instructions over 83 masks) agreed with the median set.

## 2. Hotspots

### 2.1 Shares (self cost, grouped)

| group | median9 instr | median9 **time** | worst10 instr | worst10 **time** |
|---|---|---|---|---|
| per-step vocab decode: `decode_utf8` compute | 23.1% | 13.7% | 8.1% | 4.6% |
| malloc / free (almost all from the 152k per-token `std::vector<uint32_t>` built by decode and destroyed at the end of apply) | ~35% | **25.6%** | ~13% | 9.5% |
| `llama_grammar_apply_impl` self (candidate setup, vector-of-vectors bookkeeping) | 6.2% | 12.8% | 2.2% | 4.2% |
| `llama_vocab::is_eog` (a `std::set` lookup per vocab token per step) + `token_to_piece` | 5.6% | 2.3% | 2.0% | ~0.5% |
| **per-stack sweep**: `llama_grammar_reject_candidates_for_stack` self | 12.3% | **31.2%** | 32.3% | **63.0%** |
| `llama_grammar_match_char` | 16.3% | 6.9% | 40.1% | 14.5% |
| `llama_grammar_advance_stack` (incl. the std::set de-dup from #18604) | <0.5% | <0.5% | 0.67% | <0.5% |

Inclusive view, median9: everything that is decode-related (decode + its allocations + the frees of
those vectors) is **58.7% of instructions**; all per-stack work (`llama_grammar_reject_candidates`
inclusive) is 29.5%.

### 2.2 Two cost regimes (from the baseline fit)

Per step: **cost ~= 25 ms (fixed, every vocab token) + ~3-4 ms per live stack** (baseline.md). At the
median step there are 3 stacks, so the fixed vocab work dominates; the p99 tokens have a median of
156 stacks, so the per-stack sweep dominates. The two halves of the profile above are those two
regimes.

### 2.3 Why time and instructions disagree: the sweep is memory-bound

`reject_candidates_for_stack` is 12% of instructions but 31% of time on the median set (63% on the
worst set), while `match_char` is the reverse (16% of instructions, 7% of time). For every live
stack, the top-level call walks the whole candidate list (~152k entries x 32 bytes = 4.8 MB), reads
each candidate's first code point through a pointer into its own small heap block (152k scattered
allocations), and copies almost every candidate into a new `rejects` vector (another 4.8 MB),
because ~99.9% of tokens are rejected at the first character. That is ~10 MB of streaming plus
152k dependent loads per stack, far beyond L2. The worst grammars do this 100-600 times per step.

### 2.4 What creates hundreds of stacks (checked, not assumed)

Decoding the trace text at the highest-stack steps:

- `Github_ultra/o46322` (201 properties, none required) peaks at 600 stacks right after
  `,\n  ` between properties; `JsonSchemaStore/rustfmt` (78 optional properties) peaks at 231 at
  the same kind of position. llama.cpp's json-schema-to-grammar emits optional properties as a chain
  `( "," space next-kv )? next-rest`, so after each comma every remaining property is a live
  alternative, about 3 stacks per property (whitespace continuation states x key start):
  600 ~= 200 x 3, 231 ~= 77 x 3.
- `JsonSchemaStore/mason-registry` peaks at 505 stacks inside `"licenses": [ ... ]`, whose items are
  an SPDX license enum: one stack per enum string.

## 3. Candidates

Ceilings below are "if this part of the cost went to zero"; the realistic estimate is lower and
says why. Time shares (perf) are used for ceilings because that is what users wait for;
instruction shares are the tie-breaker.

| # | candidate | targets | ceiling | realistic estimate | patch size | upstream overlap |
|---|---|---|---|---|---|---|
| **P1** | **decode all vocab tokens into one flat code-point buffer per apply call** instead of one `std::vector<uint32_t>` per token | median (fixed term) | median -38% (malloc/free 25.6% + most of apply self 12.8%) | **median -20 to -30%**, p99 -5 to -10% (plus possibly better locality in every sweep, since code points become contiguous) | ~20-30 lines in `llama_grammar_apply_impl` + a decode variant that appends to a buffer; no new state, no API change | #4210/#4330 (2023, merged) only added `reserve`. #6811 (closed 2024) cached decoded code points *across steps* (vocab-sized persistent memory, lazy init, mutex) and was dropped for lack of end-to-end gain. P1 keeps nothing across steps, so none of #6811's memory/concurrency concerns apply. Not a duplicate. |
| **P2** | **per-stack first-character pre-filter at the top level**: keep a compact array of each candidate's first code point; for each stack, send only candidates whose first code point matches the stack's top char element into the existing `reject_candidates_for_stack`, and mark survivors as accepted (a token is legal iff any stack accepts it) instead of copying the whole reject list from stack to stack | p99 (per-stack term), also median | worst10: sweep + match_char = 77.5% of time; median9: 38% | **p99 -40 to -60%**, median -15 to -25%. Each stack's sweep becomes a scan of a 600 KB array instead of ~10 MB of copying plus 152k pointer loads | ~40-60 lines, all inside the static `llama_grammar_reject_candidates` / `apply_impl`; `llama_grammar_reject_candidates_for_stack` (used by test-llama-grammar) is unchanged | #28371 (open since 2026-09, no maintainer review) attacks the same stack explosion with a persistent memo cache for small candidate lists; different mechanism (no cache, no new state) but a reviewer will ask how they relate. #30476 (open) changes how json-schema-to-grammar emits optional properties, which may reduce stack counts at the source. |
| P3 | 128-entry ASCII match table per stack top, built once per call when the candidate list is large | match_char | worst10 14.5% time (40% instr) | -5 to -10% time; mostly subsumed by P2 (P2's pre-filter is where most match_char calls happen) | ~10-15 lines | none found |
| P4 | cache the end-of-generation test per step instead of a `std::set` lookup per vocab token | is_eog | 1.7% time | ~-1.5% | ~5 lines | none found |
| P5 | persistent decoded code-point cache across steps | decode compute | additional -14% median after P1 | additional -8 to -12% | ~40 lines + ~5 MB per grammar for a 152k vocab, copied on every `llama_grammar_clone_impl` | **#6811 closed** with exactly this idea; deprioritized |
| P6 | cheaper stack de-dup in `advance_stack` (recover #18604's ~13%) | advance_stack | <1% (0.67% of instructions even on the 600-stack grammars) | ~0 | - | #18604 |
| P7 | skip `advance_stack` + recursion when no candidate passed the first char | advance_stack | <1% | ~0 | ~3 lines | - |

**Rejected after profiling:** P6 and P7. The brief suggested recovering #18604's de-dup cost; on
this workload `advance_stack` is under 1% of instructions even for the 600-stack grammars,
because the expensive part of a stack is not creating it but sweeping the vocab once per stack.
P4 is below the ~3% bar. P5 overlaps a closed upstream PR and adds per-grammar memory.

## 4. End-to-end context (does the full mask matter in real generation?)

`scripts/run_e2e.sh`: 28 grammars (all 8 built-in + 2 per JSONSchemaBench subset), Qwen2.5-0.5B
Q4_K_M, 4 threads, nothing else running, each grammar generated twice (seed 1234, up to 256 tokens).
Full table: [`e2e/base/qwen05_summary.md`](e2e/base/qwen05_summary.md).

| | ms / token | vs model decode |
|---|---|---|
| model decode (forward pass) | **27.2** | - |
| grammar, full mask every step (`grammar_first = true`: speculative-decoding draft sampling, examples/speculative) | **35.6** | **131%** |
| grammar, default path (`common_sampler_sample`: check the sampled token, full mask only if it is rejected) | **1.5** | **6%** |
| steps on the default path that needed the full mask | 181 / 3544 = **5.1%** | per grammar: median 5%, max 64% (JsonSchemaStore cloudformation) |

What this means for the candidates:

- **On the default path the full mask is rare**, so mask speedups move end-to-end tokens/s only a
  little: a 40% cheaper mask would cut 1.5 ms to ~0.9 ms per token, about **+2% tokens/s**. This is the
  same reason upstream #6811 was closed in 2024; it is still true.
- **Where the full mask runs every step it costs more than the model itself** (131% of decode here,
  because the 151k vocab is large and the model is small). There a 40% cheaper mask is a projected
  **~+30% tokens/s** (27.2 + 35.6 ms -> 27.2 + 21.4 ms per token). This applies to speculative-decoding
  draft sampling with a grammar, to any caller that applies the grammar before the chain, and more
  generally to small models with large vocabs.
- **Latency spikes:** a rejection at a high-stack position costs a full mask at that position, which
  is 0.2-1.6 s on the worst grammars (baseline worst-10 p99..max). P2 targets exactly those steps.
- Grammars with frequent rejections (cloudformation 64%, cluster 30%, Github_trivial o21828 36%)
  pay the full-mask cost on the default path too.

## 5. GATE

**Criterion** (from the brief): continue only if at least one non-duplicate candidate plausibly gives
>= 15% lower median per-token grammar cost, or >= 25% lower p99, with a small, simple patch.

**Verdict: PASS.**

- **P1 (flat code-point buffer)** plausibly gives **-20 to -30% median** (malloc/free alone is 25.6%
  of time on the median set) with ~20-30 lines and no new state. Not a duplicate: #6811 cached across
  steps and was closed; P1 changes only how one apply call stores its temporary decode.
- **P2 (per-stack first-character pre-filter)** plausibly gives **-40 to -60% p99** (the per-stack
  sweep + match_char are 77.5% of time on the worst set) with ~40-60 lines inside static functions.
  Not a duplicate of #28371 (persistent memo cache), but it targets the same symptom, so the
  write-up must compare them.
- Both are checkable bit-for-bit with `grammar_replay --check` on all 23,448 steps.

**Recommendation for Phase 3 (waiting for your go-ahead):**

1. P1 first (best value per line, lowest risk).
2. P2 second (largest p99 win; more code, so it must stay readable and be measured separately).
3. P3 (ASCII table) only if it still gives >= 3% after P2; P4-P7 not pursued (reasons in section 3).
4. Report every result two ways: the replay metric the gate is defined on (full mask), and the
   end-to-end tokens/s on both the default path and the full-mask-every-step path, so the
   end-to-end caveat above is visible next to every headline number.

**Risks to watch:** P1 stores raw pointers into one buffer, so the buffer must never reallocate
after pointers are taken (the code-review skill's "reserved buffers" and "stored pointers" rules);
P2 changes how the reject list is built, so its correctness argument (each candidate's decision for
a stack is independent of the other candidates) must be written down and backed by the bit-exact
check, and `llama_grammar_reject_candidates_for_stack` must keep its exact behavior because
test-llama-grammar calls it directly.
