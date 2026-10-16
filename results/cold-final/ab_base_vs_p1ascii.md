## Paired A/B: `p1ascii` vs `base` (cold-final)

19 grammars x 2 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / p1ascii) | **1.250x** | 1.111 .. 1.384 |
| median per-token cost, p1ascii / base | **0.742** (-25.8%) | 0.668 .. 0.859 |
| pooled p99, p1ascii / base | **1.926** (+92.6%) | 0.881 .. 1.959 |
| total replay time, p1ascii / base | 1.064 (+6.4%) | 0.768 .. 1.293 |

Per-grammar speedup: 16 faster, 3 slower; range 0.625x .. 1.604x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Github_ultra__o46322`: 0.625x [0.621 .. 0.629], 234.5 -> 375.5 ms/token
- `jsb__JsonSchemaStore__rustfmt`: 0.756x [0.726 .. 0.787], 120.0 -> 158.4 ms/token
- `jsb__Snowplow__sp_408_Normalized`: 0.806x [0.783 .. 0.829], 76.4 -> 94.5 ms/token
- `jsb__JsonSchemaStore__mason-registry`: 1.147x [1.121 .. 1.174], 75.6 -> 65.8 ms/token
- `jsb__Github_hard__o6347`: 1.166x [1.098 .. 1.238], 69.4 -> 59.4 ms/token
- `jsb__Github_medium__o47263`: 1.503x [1.488 .. 1.519], 26.7 -> 17.8 ms/token
- `jsb__Github_hard__o21418`: 1.532x [1.511 .. 1.554], 25.0 -> 16.3 ms/token
- `jsb__Github_trivial__o58627`: 1.563x [1.515 .. 1.613], 33.3 -> 21.2 ms/token
- `jsb__Github_medium__o44462`: 1.598x [1.533 .. 1.665], 25.2 -> 15.7 ms/token
- `jsb__Github_ultra__o6369`: 1.604x [1.504 .. 1.710], 26.8 -> 16.6 ms/token
