# Baseline: grammar-constrained sampling cost in llama.cpp (CPU)

Status: replay results final (5 runs). Sections marked FILL (end to end, TinyLlama) are filled when those runs finish.

## What is measured

The cost of llama.cpp's built-in GBNF grammar engine per generated token, isolated from model
inference:

1. **Record** (once per grammar): generate one constrained output with a real model and save the
   token ids (`bench/grammar_record.cpp`, seed 1234, top-k 40, temp 0.7, max 256 tokens).
2. **Replay** (many times): load only the vocab (`vocab_only`), build the grammar, and for every
   recorded token run the **full-vocab mask** (`llama_grammar_apply_impl` over all 151,936
   tokens) and then **accept** the token (`llama_grammar_accept_impl`). Only these two calls are
   timed (`bench/grammar_replay.cpp`). Per-token cost = mask + accept.

The full mask on every step is the slow path. In normal `common_sampler` use the full mask
runs only when the sampled token is rejected; in speculative-decoding draft sampling
(`grammar_first = true`) it runs every step. Section "End to end" measures how often it runs.

## Setup

| | |
|---|---|
| CPU | Intel Xeon @ 2.80 GHz (cloud VM), 4 vCPU, AVX-512, 15 GB RAM, no HW perf counters |
| OS / compiler | Linux 6.18 guest, Ubuntu 24.04, GCC 13.3, CMake 3.28 |
| llama.cpp | `620d2225` (master, 2026-10-15), Release (`-O3`), static, ccache; libllama compiled without `-march`, ggml-cpu with `-march=native` |
| Vocab / traces | Qwen2.5-0.5B-Instruct Q4_K_M (BPE, 151,936 tokens); TinyLlama-1.1B-Chat Q4_K_M (SPM, 32,000 tokens) FILL if run |
| Replay | one process at a time (`-j 1`), nothing else running during runs 1-5 |

## Workload

- **(a) built-in grammars**: all 8 `grammars/*.gbnf` (arithmetic, c, chess, english, japanese,
  json, json_arr, list), each with a short task prompt (`data/prompts/gbnf__*.txt`).
- **(b) JSONSchemaBench**: test split of all 10 subsets, 20 schemas per subset sampled with a
  fixed seed (pandas `random_state=42`), converted to GBNF with llama.cpp's own
  `json_schema_to_grammar` (same entry point as `--json-schema` and the server). 0 conversion
  failures. 3 sampled JsonSchemaStore schemas share the dataset id `project-1`; one is kept
  (DECISIONS D1.11). **198 schemas.**
- Total **206 grammars, 23,448 replayed tokens**. 161 generations ended naturally (EOG), 45 hit the
  256-token cap; truncated traces are still valid prefixes.

## Results (Qwen2.5 vocab, 151,936 tokens)

Full tables and charts: [`baseline-qwen05/summary.md`](baseline-qwen05/summary.md). 5 runs, one process
at a time, nothing else running. Values: median over runs [min .. max over runs].

| metric | value |
|---|---|
| **median per-token grammar cost** (median over grammars of each grammar's mean) | **31.3 ms** [29.0 .. 34.1] |
| mean over all tokens | 40.3 ms [35.5 .. 44.8] |
| pooled p50 over all tokens | 31.4 ms [27.7 .. 34.3] |
| **pooled p99 over all tokens** | **199 ms** [163 .. 251] |
| total replay time for the workload | 945 s [832 .. 1050] |
| accept share of the time | 0.1% (mask is 99.9%) |

**Run-to-run drift.** Runs got steadily faster: total 1050, 1026, 945, 886, 832 s (median per-grammar
cost 34.1 -> 29.0 ms). This is a monotonic drift of the VM (host load or clock), not jitter, ~20% over
75 minutes. Absolute numbers on this VM are therefore only good to about +-10%; every A/B comparison
in Phase 3 interleaves base and patched runs (ABBA order) and compares run i with run i.

Per subset (median over grammars of each grammar's mean, ms/token): gbnf 34.3, Github_trivial 31.4,
Github_easy 31.7, Github_medium 34.8, Github_hard 31.1, Github_ultra 36.8, Glaiveai2K 24.2,
JsonSchemaStore 38.0, Kubernetes 29.1, Snowplow 30.1, WashingtonPost 32.6. The typical cost is
similar everywhere; the subsets differ in their tails.

![CDF of per-token cost](baseline-qwen05/charts/cdf_per_token.png)

### Where the cost comes from: a fixed vocab sweep plus a per-stack sweep

- At the median step the grammar allows only ~0.1% of the vocab (about 150 of 151,936 tokens), yet a
  step with a single live stack still costs ~20 ms. That cost is paid for every vocab token on every
  step, whatever the grammar.
- Live stacks per step: median 3, p90 7, p99 77, max 600.
- A least-squares fit over steps with <= 10 stacks gives **cost ~= 25 ms + 4.0 ms x stacks**
  (over all steps: 28 ms + 2.8 ms x stacks). The fixed term alone is ~57% of all replay time.
- Steps with >= 50 stacks are 1.6% of tokens but 16% of the time; tokens at or above the pooled p99
  have a median of 156 stacks. **The median is driven by the fixed per-vocab work; the p99 is driven
  by the stack count.**

![cost vs stacks](baseline-qwen05/charts/stacks_vs_cost.png)

## Worst 10 grammars

Median over 5 runs (ms per token). Full list: [`baseline-qwen05/per_grammar.csv`](baseline-qwen05/per_grammar.csv).

| grammar | tokens | mean | p50 | p99 | max | peak stacks |
|---|---|---|---|---|---|---|
| jsb Github_ultra o46322 | 215 | 275.7 | 31.9 | 1525 | 1643 | 600 |
| jsb JsonSchemaStore rustfmt | 184 | 129.8 | 29.1 | 588 | 706 | 231 |
| jsb JsonSchemaStore mason-registry | 162 | 105.2 | 33.1 | 934 | 1064 | 505 |
| jsb Github_hard o6347 | 126 | 91.7 | 55.3 | 658 | 734 | 216 |
| gbnf english | 113 | 90.9 | 86.8 | 135 | 139 | 5 |
| jsb Snowplow sp_408 | 254 | 72.8 | 27.3 | 338 | 378 | 159 |
| jsb Github_ultra o17072 | 89 | 72.6 | 36.1 | 467 | 475 | 182 |
| jsb JsonSchemaStore project-1 | 256 | 59.1 | 42.2 | 249 | 298 | 111 |
| jsb WashingtonPost wp_17 | 256 | 57.2 | 40.5 | 336 | 430 | 144 |
| jsb Github_hard o36011 | 256 | 54.2 | 47.1 | 211 | 242 | 75 |

Nine of the ten are stack-heavy schemas (75-600 peak stacks): their median token is ordinary (~30 ms)
but a few positions with hundreds of live stacks cost 0.3-1.6 s (which grammar constructs create
those stacks is examined in profile.md). `english.gbnf` is the exception: few stacks, but its
char classes accept most of the vocab at every char, so the recursion walks almost every token's full
text.

## Grammar cost vs model compute (end to end)

FILL: from `scripts/run_e2e.sh` (28-grammar subset, real model, 4 threads).

## Reference ceiling: llguidance

`bench/llg_replay.cpp` replays the same traces through llguidance v1.0.1 (llama.cpp's optional
`-DLLAMA_LLGUIDANCE=ON` backend; built in ~2 min, so it was in budget), driven by the original
JSON schema, timing the same apply + accept calls on the same 151,936-token candidate array.

| | value |
|---|---|
| schemas measured | 162 of 198 |
| unsupported by llguidance defaults | 36 (unimplemented keys 14, `oneOf` 10, unknown `format` 10, schema too large 2) |
| diverged | 39: llguidance disallowed a recorded token (mostly whitespace rules differ), replay stops there |
| median of per-grammar mean | **0.41 ms / token** |
| median of per-grammar p99 | 0.67 ms / token |
| worst per-grammar mean | 2.6 ms / token |
| matcher init (median / p90) | 1.4 ms / 6.3 ms (first init also builds the tokenizer: ~1 s) |

Caveats: llguidance masks are not comparable bit-for-bit (different JSON-schema semantics and
whitespace rules); the llg run was a single pass at `nice 19` while baseline run 3 was in
progress on another core. It is a reference point for "what fast looks like", not an A/B.
