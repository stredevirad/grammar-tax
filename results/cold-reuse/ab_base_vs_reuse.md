## Paired A/B: `reuse` vs `base` (cold-reuse)

19 grammars x 2 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / reuse) | **1.381x** | 1.336 .. 1.426 |
| median per-token cost, reuse / base | **0.748** (-25.2%) | 0.653 .. 0.807 |
| pooled p99, reuse / base | **0.831** (-16.9%) | 0.686 .. 0.952 |
| total replay time, reuse / base | 0.734 (-26.6%) | 0.706 .. 0.758 |

Per-grammar speedup: 19 faster, 0 slower; range 1.223x .. 1.580x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__JsonSchemaStore__mason-registry`: 1.223x [1.217 .. 1.228], 86.0 -> 70.3 ms/token
- `gbnf__arithmetic`: 1.240x [1.187 .. 1.295], 30.2 -> 24.4 ms/token
- `jsb__JsonSchemaStore__project-1`: 1.253x [1.223 .. 1.283], 55.1 -> 44.0 ms/token
- `jsb__Github_hard__o6347`: 1.287x [1.270 .. 1.305], 68.1 -> 52.9 ms/token
- `jsb__Github_trivial__o58627`: 1.290x [1.227 .. 1.357], 26.4 -> 20.5 ms/token
- `jsb__JsonSchemaStore__rustfmt`: 1.478x [1.476 .. 1.480], 126.3 -> 85.5 ms/token
- `jsb__WashingtonPost__wp_115_Normalized`: 1.485x [1.292 .. 1.707], 31.5 -> 21.0 ms/token
- `jsb__Github_medium__o47263`: 1.503x [1.490 .. 1.515], 25.5 -> 16.9 ms/token
- `jsb__Github_medium__o44462`: 1.509x [1.499 .. 1.519], 22.8 -> 15.1 ms/token
- `jsb__Github_hard__o21418`: 1.580x [1.580 .. 1.581], 25.8 -> 16.3 ms/token
