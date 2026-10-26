# quick_verify.sh runs

[`scripts/quick_verify.sh`](../../scripts/quick_verify.sh) is the short independent check of the headline result:
it builds upstream at the pinned SHA and the final series (0001-0003), checks that the masks are bit-identical on
every step of every recorded trace (23,448 Qwen + 21,202 TinyLlama steps), then runs a 3-run interleaved A/B on
the 20 grammars in [`data/quick_subset.txt`](../../data/quick_subset.txt). Choices: DECISIONS.md D3.18.

## Subsets

| subset | grammars | rule | tokens | `ab-final` base replay, run 1 | expected from `ab-final` |
|---|---|---|---|---|---|
| v1 (2026-10-17, archived) | [v1-subset.txt](v1-subset.txt) | 19 grammars at quantiles (i + 0.5) / 19 of the `ab-final` baseline per-token cost, plus o46322 | 2,863 | 120.3 s | 1.432x |
| v2 (current) | [`data/quick_subset.txt`](../../data/quick_subset.txt) | the 206 grammars in the same order, cut into 19 equal-count bins; from each bin the grammar with the cheapest trace that still has >= 32 tokens; plus o46322 | 1,259 | 71.2 s | 1.488x |

"Expected" is the geometric mean of the `ab-final` per-grammar speedups (5 runs, 2026-10-16) over the subset's
grammars; the full workload is 1.470x. v2 replaced v1 because v1 took 29 min 47 s on a host with the study's CPU
name, at the edge of the ~30 min goal. Re-deriving v2 from the committed `ab-final` data with the rule above gives
the file exactly. o46322 (600 live stacks) is 215 of the 1,259 v2 tokens (17%; 7.5% of v1), so it dominates the
pooled p99.

## Runs

| run | subset | CPU | OS, compiler | total | builds / mask check / A/B | masks | speedup [95% CI] | faster |
|---|---|---|---|---|---|---|---|---|
| [v1-cpu-2p10ghz](v1-cpu-2p10ghz/) (2026-10-17) | v1 | Xeon @ 2.10GHz (model 207), 4 vCPU | Linux VM, GCC 13.3 | 19 min 25 s | 2.2 / 8.3 / 8.9 min | identical | 1.545x [1.490, 1.606] | 20 / 20 |
| [v1-cpu-2p80ghz](v1-cpu-2p80ghz/) (2026-10-17) | v1 | Xeon @ 2.80GHz (model 85; same CPU name as the study's runs), 4 vCPU | Linux VM, GCC 13.3 | 29 min 47 s | 3.5 / 12.5 / 13.6 min | identical | 1.566x [1.496, 1.633] | 20 / 20 |
| [v2-core-ultra-5-125h-wsl2](v2-core-ultra-5-125h-wsl2/) (2026-10-26) | v2 | Core Ultra 5 125H laptop, WSL2 (18 vCPU) | Debian 12, GCC 12.2 | 17 min 29 s | 2.2 / 9.4 / 5.9 min | identical | **1.590x [1.556, 1.626]** | 20 / 20 |

The v1-cpu-2p10ghz and v2 runs had no ccache (not recorded for v1-cpu-2p80ghz). "identical" covers all 44,650
trace steps plus the subset's own A/B replays (2,863 or 1,259 steps).

## What the runs say

- **Correctness reproduces everywhere.** Masks were bit-identical on every step in all three runs, including a
  different compiler (GCC 12.2), glibc (2.36) and CPU family (AVX2 laptop part, no AVX-512).
- **The speedup reproduces, and comes out larger than `ab-final` for the same grammars.** Every run was 7-9% above
  what `ab-final` gives for its subset: 1.545x and 1.566x vs 1.432x (v1), 1.590x vs 1.488x (v2). That includes the
  v1 run on a host with the study's CPU name, so it is not (only) the CPU. The cause is not established.
  `gbnf__japanese`, the weakest grammar in `ab-final` (1.03x), measured 1.33x on 2026-10-17 when replayed alone,
  1.36x after its 4 `ab-final` predecessors, and 1.41x with the study's own binaries (1.31x with the fresh
  quick_verify builds; 3 rounds each, [japanese-context.tsv](japanese-context.tsv),
  [japanese-binaries.tsv](japanese-binaries.tsv)): neither the replay context nor the binaries bring it back to
  1.03x. Read the quick check as "the speedup reproduces", not as a sharper number than the 5-run `ab-final` A/B.
- **On the laptop the gap sits in a few grammars, led by the deep-stack ones.** Per grammar, laptop / `ab-final`
  speedup is 0.96-1.09 for 16 of the 20 grammars (geomean +2.7%); the other four are `wp_95` and `wp_120` (+20%),
  o17072 (182 live stacks, +21%) and o46322 (600 stacks, +40%: 1.24x in `ab-final`, 1.36x / 1.33x in the v1 runs,
  1.735x here, 233.0 -> 131.1 ms/token, 1.667x .. 1.777x over the 3 runs). The deep-stack case is the memory-bound
  per-stack sweep (`results/profile.md`), the part most likely to depend on the memory hierarchy. In the v1 runs
  the largest per-grammar gaps were elsewhere (`gbnf__japanese` +30%). Because o46322 is 17% of the v2 tokens, the
  laptop's pooled p99 change (-42.4%) is much larger than the full workload's in `ab-final` (-17.9%).
- **v2 run conditions** ([environment.txt](v2-core-ultra-5-125h-wsl2/environment.txt),
  [cpu.txt](v2-core-ultra-5-125h-wsl2/cpu.txt)). Laptop on AC power (49% -> 65% charge), Windows "Balanced"
  plan; Debian 12 under WSL2 with GCC 12.2 and glibc 2.36 (the study: GCC 13.3, glibc 2.39). Median per-token
  costs were 27% (baseline) and 31% (final) lower than in `ab-final` for the same grammars. The Core Ultra 5 125H
  mixes performance and efficiency cores and the A/B replays are single-threaded, so a replay can land on either;
  even so, the per-grammar spread over the 3 runs (median max/min 1.13; widest `wp_120`, 1.443x .. 2.128x) is
  close to the v1 VM runs' (1.11 and 1.07). Python 3.11 (Debian 12): `scripts/requirements.txt` pins numpy 2.5.4,
  which needs Python >= 3.12, so this run used numpy 2.4.6 with the other pins unchanged (pip freeze in
  environment.txt). The Hugging Face downloads in `scripts/fetch_models.sh` were interrupted (HTTP/2 stream
  resets) and resumed; both files passed the sha256 check.
