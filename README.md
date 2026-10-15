# grammar-tax

A performance study of llama.cpp's grammar-constrained (GBNF) sampling on the CPU:
a deterministic record/replay benchmark, profiling, and (if the gate passes) small measured
optimizations stored as patch files. Work in progress; see `PHASE_LOG.md` for status.

## Pinned upstream

| | |
|---|---|
| Submodule | `third_party/llama.cpp` |
| Upstream | https://github.com/ggml-org/llama.cpp |
| Pinned SHA | `620d2225cc436ab0b001e3200e590e3b286ef9ab` (master, 2026-10-15, "llama.cpp : bump version to 0.7.0 (#30603)") |

The submodule is never modified in a committed state. Changes live in `patches/` and are applied
to a separate worktree by `scripts/apply_patches.sh`.

## Layout

- `bench/` - benchmark tools (CMake wrapper project that builds llama.cpp + tools)
  - `grammar_record.cpp` - records one constrained generation (token ids) per grammar
  - `grammar_replay.cpp` - vocab-only replay: full-vocab mask + accept per recorded token, JSON timings, `--check` mask hashes
  - `schema_to_gbnf.cpp` - JSON schema -> GBNF via llama.cpp's own converter
  - `traces/<model>/` - recorded traces
- `data/` - sampled JSONSchemaBench schemas, converted grammars, prompts, `workload.tsv`
- `scripts/` - workload prep, recording, benchmarking, analysis
- `results/` - reports and charts
- `DECISIONS.md`, `PHASE_LOG.md` - decision log and progress log

## Reproduce (so far)

```sh
git clone --recurse-submodules <this repo> && cd grammar-tax
python3 -m venv build/venv && build/venv/bin/pip install -r scripts/requirements.txt
cmake -S bench -B build/bench-base -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache
cmake --build build/bench-base -j4 --target grammar_replay grammar_record schema_to_gbnf
scripts/fetch_models.sh            # Qwen2.5-0.5B-Instruct Q4_K_M, TinyLlama-1.1B-Chat Q4_K_M
# traces and converted grammars are committed; to regenerate:
#   scripts/fetch_jsb.sh && build/venv/bin/python -I scripts/prepare_workload.py
#   scripts/record_traces.sh models/qwen2.5-0.5b-instruct-q4_k_m.gguf qwen05
scripts/run_bench.sh               # see script header for options
```
