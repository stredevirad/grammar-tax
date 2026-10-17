## Paired A/B: `final` vs `base` (raw)

20 grammars x 3 interleaved runs; bootstrap over grammars, 2000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / final) | **1.545x** | 1.490 .. 1.606 |
| median per-token cost, final / base | **0.645** (-35.5%) | 0.598 .. 0.705 |
| pooled p99, final / base | **0.746** (-25.4%) | 0.663 .. 0.778 |
| total replay time, final / base | 0.698 (-30.2%) | 0.650 .. 0.720 |

Per-grammar speedup: 20 faster, 0 slower; range 1.342x .. 1.814x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__japanese`: 1.342x [1.306 .. 1.366], 22.8 -> 17.4 ms/token
- `jsb__Github_ultra__o46322`: 1.356x [1.321 .. 1.392], 189.6 -> 141.3 ms/token
- `jsb__Github_hard__o6347`: 1.365x [1.291 .. 1.409], 54.3 -> 39.0 ms/token
- `jsb__Kubernetes__kb_929_Normalized`: 1.394x [1.316 .. 1.512], 19.2 -> 14.0 ms/token
- `jsb__WashingtonPost__wp_7_Normalized`: 1.477x [1.397 .. 1.622], 20.2 -> 14.4 ms/token
- `jsb__Github_trivial__o7488`: 1.590x [1.474 .. 1.754], 19.3 -> 12.4 ms/token
- `jsb__Glaiveai2K__calculate_area_f9afae07`: 1.714x [1.467 .. 2.135], 15.0 -> 9.3 ms/token
- `jsb__Github_easy__o12214`: 1.737x [1.384 .. 2.210], 20.9 -> 11.2 ms/token
- `jsb__Github_trivial__o46562`: 1.805x [1.529 .. 2.285], 14.0 -> 8.3 ms/token
- `jsb__Github_easy__o6120`: 1.814x [1.674 .. 1.943], 17.5 -> 9.5 ms/token
