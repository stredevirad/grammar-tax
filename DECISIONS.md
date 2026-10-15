# Decisions log

Each entry: what was decided, why, and what it affects. Newest at the bottom of each section.

## Phase 0: setup

- **D0.1 Remote repo was fully empty** (no README on the remote, no commits). The first commit on branch `claude/nice-bardeen-at3kfm` creates the repo content. README.md is written from scratch.
- **D0.2 Submodule pin.** `third_party/llama.cpp` pinned to master `620d2225cc436ab0b001e3200e590e3b286ef9ab` ("llama.cpp : bump version to 0.7.0 (#30603)", 2026-10-15). Added with `--depth 1` to save disk/time; `git submodule update --init` still works because GitHub serves reachable SHAs.
- **D0.3 Build layout.** One CMake wrapper project (`bench/CMakeLists.txt`) does `add_subdirectory(LLAMA_SRC)` and adds the bench tools, so each variant (baseline / patched) is one build tree: `build/bench-base` (pristine submodule) and later `build/bench-patched` (a git worktree of the submodule at `build/src-patched` with `patches/*.patch` applied). Release, static libs, ccache, `-j4`, only needed targets. Server/app/examples/HTML/OpenSSL off.
- **D0.4 GGML_NATIVE left ON** (default). Both A and B builds use the same flags on the same machine, so this does not bias A/B. Absolute numbers are machine-specific.
- **D0.5 Tooling that works in this VM** (4 vCPU, 15 GB RAM, kernel 6.18 firecracker guest):
  - ccache 4.9.1, valgrind 3.22 (callgrind), hyperfine 1.18: preinstalled, work.
  - `perf` (installed `linux-tools-generic`, binary `/usr/lib/linux-tools/6.8.0-147-generic/perf`): **only software events** work (`cpu-clock`, `task-clock`). No hardware PMU in the guest, so `cycles`/`instructions` are "not supported". `perf record -e cpu-clock -g` sampling works. Callgrind is the source of noise-free instruction counts.
  - Python analysis deps in a venv (`build/venv`, pinned in `scripts/requirements.txt`): pandas, pyarrow, matplotlib, numpy.
- **D0.6 Baseline tests** (pristine submodule, Release): test-grammar-parser, test-grammar-integration, test-llama-grammar, test-json-schema-to-grammar all exit 0. (test-grammar-llguidance needs `-DLLAMA_LLGUIDANCE=ON`, not built.)

## Phase 1: harness

- **D1.1 What is timed.** The replay tool calls the internal API `llama_grammar_apply_impl` (full mask over the whole vocab) and `llama_grammar_accept_impl` per recorded token. This is exactly what `llama_sampler_grammar_apply/accept` call, minus a pointer hop. The internal API is used (header `src/llama-grammar.h`, like the upstream tests do) so the tool can also read the number of live stacks.
- **D1.2 Full mask every step is the worst case, not the common case.** `common_sampler_sample` first samples without the grammar and only checks the one chosen token (cheap); the full-vocab mask runs only when that token is rejected (or when `grammar_first` is set). So the replay measures the cost of the slow path. Real end-to-end cost per token = (fraction of steps that need the full mask) x (mask cost) + (single-token check). Reported this way in baseline.md. The trace recorder applies the full mask every step (grammar first) so traces are always grammar-valid.
- **D1.3 Recording settings.** Chat template from the GGUF, system prompt "answer only in requested format", top-k 40, temp 0.7, seeded dist (seed 1234), max 256 generated tokens, 120 s wall cap per grammar. Truncated traces are fine: replay only needs a valid prefix. Recorded with 2 parallel jobs x 2 threads (grammar is single threaded). Trace files record the metadata line (model, seed, n_prompt, n_gen, complete, timings).
- **D1.4 Schema sample.** JSONSchemaBench HF dataset `epfl-dlab/JSONSchemaBench` at revision `5bd0f46`, test split of the 10 subsets (Github_trivial/easy/medium/hard/ultra, Glaiveai2K, JsonSchemaStore, Kubernetes, Snowplow, WashingtonPost), 20 per subset with pandas `sample(random_state=42)` = 200 schemas. Prompt includes the schema (pretty-printed, truncated to 6000 chars).
- **D1.5 Conversion** uses llama.cpp's C++ `json_schema_to_grammar(common_json)` (same entry point as `--json-schema` and the server), via `bench/schema_to_gbnf`. All 200 sampled schemas converted (0 failures; `data/conversion_failures.tsv` is empty apart from the header). The converter is tolerant: unsupported keywords are dropped with warnings rather than failing.
- **D1.6 Mask hash.** FNV-1a 64 over the per-token reject byte (logit == -inf) in vocab order, one hash per step, plus `n_allowed`. Two builds agree bit-for-bit iff every line of the `--check` file matches.
- **D1.7 Callgrind build.** valgrind 3.22 aborts on AVX-512 (EVEX) instructions emitted in ggml-cpu by `-march=native`. Profiling uses a second build `build/cg-base` with `-DGGML_NATIVE=OFF` (ggml-cpu gets AVX2/FMA only). libllama (where all grammar code lives) is compiled with plain `-O3`, no `-march`, in both builds, so the profiled grammar code is the same machine code family as the timed one. Collection is restricted with `--toggle-collect` to `llama_grammar_apply_impl` and `llama_grammar_accept_impl` so vocab loading is excluded.
- **D1.8 Background job limit.** The first recording job was stopped by the 30 min background limit after 146/208 grammars; the recorder writes a trace only when a generation finishes, and the script skips existing traces, so the rerun only re-did the 2 in-flight grammars. No partial traces exist.
- **D1.9 End-to-end subset.** `data/e2e_subset.txt` = all 8 built-in gbnf + the first 2 (by sorted id) of each JSONSchemaBench subset = 28 grammars. Each runs twice with the real model (4 threads, one job): `--mode first` (full mask every step) and `--mode resample` (mirror of `common_sampler_sample`). This measures how often the full mask is actually needed on the default path, which matters because upstream PR #6811 (codepoint cache) was closed in 2024 after the resampling fix (#7424) made the full mask rare end to end.
- **D1.10 Workload-level metrics.** "Median per-token grammar cost" = median over grammars of each grammar's mean (mask + accept) time per token. "p99" = 99th percentile over all replayed tokens pooled. Both are computed per run; reported as median over runs with [min .. max] and the run count.
- **D1.11 Duplicate ids in JSONSchemaBench.** The JsonSchemaStore test split has 7 duplicate `unique_id`s; our seeded sample drew three different schemas all named `project-1`. They mapped to the same file names, so the last one written won and the manifest had 3 identical rows. Fixed by keeping one row per name (prepare_workload.py and analyze.py both de-duplicate). The workload is therefore **206 distinct grammars**: 8 built-in .gbnf + 198 schemas (JsonSchemaStore contributes 18, every other subset 20). The baseline run that was already in flight replayed `project-1` three times; analysis drops the duplicate rows.

## Upstream overlap (read-only search of ggml-org/llama.cpp, 2026-10-15)

Searched issues/PRs via github.com pages (no comments, issues or PRs created). Relevant items:

| Item | State | What it does | Overlap with this study |
|---|---|---|---|
| #4218 "speed-up grammar sampling" | open (roadmap) | umbrella issue: stack explosion, full-vocab checks, UTF-8 decoding | context for everything below |
| #4330 pre-computed pieces + reserve (2023) | merged | piece lookup from vocab, `reserve` in decode_utf8 | already in baseline |
| #7587 cache llama_token_to_piece (2024) | merged | vocab-level piece cache | already in baseline (`token_to_piece` is 0.8% in profile) |
| #7424 fix resampling regression (2024) | merged | restores "sample first, check chosen token, full mask only on reject" | means the full mask is the slow path, not every step; measured in e2e |
| #6811 cache decoded token codepoints (2024, ochafik) | **closed, not merged** | per-token codepoint cache (vocab-sized memory, lazy init + mutex) | direct overlap with the decode_utf8 hotspot. Closed because after #7424/#7587 the author "can't measure significant improvements" end to end. Concerns raised: memory (~vocab x 2), lazy-init concurrency. Any decode-related candidate must avoid those concerns and show value on the paths where the full mask does run |
| #18604 iterative advance_stack + std::set dedup (2026-03) | merged | author measured ~13% slower on test-grammar-integration; unordered_set was slower ("constructing hash keys from pointers"); maintainer: 13% "is fine if it helps us prevent catastrophic times" | the "cheaper de-dup" candidate. Only worth pursuing if advance_stack is a real share on our workload |
| #26885 single lookup, fewer copies (2026-09) | merged | `seen.insert` instead of find+insert, move into set | already in baseline (current code) |
| #28371 memoize reject_candidates_for_stack (open since 2026-09-04) | **open**, no maintainer review | persistent memo cache in llama_grammar for candidate lists <= 8 | overlaps with any candidate that caches reject results across steps; skip that idea |
| #28372 compact candidate array after rejection | closed by author (1-PR limit) | removes rejected tokens from cur_p so later samplers do less work | outside grammar cost (downstream samplers); skip |
| #30172 binary search in grammar clone (open) | open | clone cost for speculative decoding | not on our measured path; skip |
| #30117 multi-token matching (open) | open | feature | unrelated |
| #27675 slow decode with large tool sets | closed, fixed by #27679 | lazy-grammar trigger search | unrelated to mask cost |
| #9369 refactor llama_vocab / tokenizer construction | **closed (fixed 2024-09)** | the tokenizer-construction overhead suggested as a fallback target in the brief | no longer a valid alternative target |
