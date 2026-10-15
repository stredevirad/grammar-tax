# Baseline: grammar-constrained sampling cost in llama.cpp (CPU)

Status: DRAFT. Sections marked FILL are completed when all baseline runs finish.

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
- Total **206 grammars, FILL tokens**. 161 generations ended naturally (EOG), 45 hit the
  256-token cap; truncated traces are still valid prefixes.

## Results (Qwen2.5 vocab, 151,936 tokens)

FILL: table from `results/baseline-qwen05/summary.md` (median over 5 runs, [min .. max]).

## Worst 10 grammars

FILL

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
