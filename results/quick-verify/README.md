# quick_verify.sh runs

Each folder is one from-scratch run of [`scripts/quick_verify.sh`](../../scripts/quick_verify.sh): builds, the
bit-exact mask check on all 44,650 trace steps (23,448 Qwen + 21,202 TinyLlama), and a 3-run interleaved A/B
on a 20-grammar subset. Every run below found the masks bit-identical.

| folder | subset | machine | total | subset A/B (base / final) | expected from `ab-final` |
|---|---|---|---|---|---|
| [v1-cpu-2p80ghz](v1-cpu-2p80ghz/) | [v1](v1-subset.txt) | 4-vCPU VM, Xeon @ 2.80GHz (same CPU name as the study's runs) | 29 min 47 s | **1.566x** [1.496, 1.633] | ~1.43x |
| [v1-cpu-2p10ghz](v1-cpu-2p10ghz/) | [v1](v1-subset.txt) | 4-vCPU VM, Xeon @ 2.10GHz | 19 min 25 s | **1.545x** [1.490, 1.606] | ~1.43x |
| [v2-core-ultra-5-125h-wsl2](v2-core-ultra-5-125h-wsl2/) | [v2](../../data/quick_subset.txt) | laptop, Core Ultra 5 125H, WSL2 | 17 min 29 s | **1.590x** [1.556, 1.626] | ~1.49x |

The v1 subset is 19 grammars at evenly spaced quantiles of the baseline cost plus o46322; v2 (the current
`data/quick_subset.txt`) takes the cheapest trace with >= 32 tokens from each of 19 equal-count bins, plus
o46322, so the A/B is shorter.

Paired ratios move between sessions and machines: both v1 runs came out 8-10% above what `ab-final` gave for the
same 20 grammars, and the v2 run is 7% above its expectation. The 125H is a hybrid CPU and WSL2 does not pin the
replay to one core type, which may explain wide per-grammar ranges in that run (e.g. `wp_120`, 1.443 .. 2.128).
[`japanese-context.tsv`](japanese-context.tsv) and [`japanese-binaries.tsv`](japanese-binaries.tsv) are the
checks behind the v1 `gbnf__japanese` result: replayed alone or after its `ab-final` predecessors, and with the
study's own binaries vs the quick-verify builds; the ratio moved with the machine state, not with the build or
the context.

v2 run details (toolchain, power, the numpy 2.4.6 substitution on Python 3.11, pip freeze):
[v2-core-ultra-5-125h-wsl2/environment.txt](v2-core-ultra-5-125h-wsl2/environment.txt).
