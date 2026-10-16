# Resume bullets

Worded for a **benchmarked patch** (not merged upstream). Change the wording only if a patch is actually merged,
and then link the merged PR. Every number is in `results/REPORT.md` with its spread and run count.

**Option A (one line, for a projects section)**

- Profiled llama.cpp's grammar-constrained (GBNF) sampling on CPU with a record/replay benchmark (206 grammars,
  23k tokens) and wrote 3 small C++ patches (+73 / -22 lines) that cut full-vocab mask computation by 1.47x
  (median per-token cost -33%, p99 -18%; 5 paired runs, 95% CI 1.455-1.486x) with bit-identical masks; about +2%
  end-to-end tokens/s on the default sampling path and +24% where the mask runs every step.

**Option B (three bullets)**

- Built a deterministic record/replay benchmark for llama.cpp's GBNF grammar engine (206 grammars incl. 198
  JSONSchemaBench schemas, 2 tokenizers), isolating mask computation from model inference; profiled with
  callgrind and perf and found ~35% of mask instructions were per-token heap allocation.
- Wrote 3 small patches (one decode buffer per call, an ASCII match table, reuse of per-stack reject lists) that
  make the full-vocab mask 1.47x faster across all 206 grammars (median -33%, p99 -18%), verified bit-identical
  on 44,650 trace steps plus a differential random-walk test, including partial-UTF-8 and byte-fallback edge
  cases; end-to-end gain is ~2% on the default sampling path, +24% where the mask runs every step.
- Documented negative results with paired A/B statistics (bootstrap CIs): two pre-filter designs, two micro-
  optimizations, and an upstream memoization PR; found and confirmed (valgrind) a use-after-free in that PR's
  cache, and fixed a cold-start page-fault regression (up to +60%) that my first patch exposed in glibc's allocator.

**Do not claim:** "merged into llama.cpp", "47% faster inference", or "speeds up llama-server by 1.47x". The
1.47x is mask computation; default-path end-to-end throughput moved by ~2%.
