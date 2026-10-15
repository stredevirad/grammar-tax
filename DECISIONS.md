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
