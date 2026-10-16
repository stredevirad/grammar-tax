## Paired A/B: `p2b` vs `base` (ab-v139)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / p2b) | **1.318x** | 1.300 .. 1.337 |
| median per-token cost, p2b / base | **0.767** (-23.3%) | 0.738 .. 0.781 |
| pooled p99, p2b / base | **0.746** (-25.4%) | 0.655 .. 0.907 |
| total replay time, p2b / base | 0.772 (-22.8%) | 0.756 .. 0.787 |

Per-grammar speedup: 206 faster, 0 slower; range 1.026x .. 1.742x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__english`: 1.026x [0.873 .. 1.147], 73.3 -> 74.6 ms/token
- `jsb__WashingtonPost__wp_61_Normalized`: 1.077x [1.006 .. 1.156], 40.3 -> 35.9 ms/token
- `jsb__Github_trivial__o69498`: 1.082x [0.816 .. 1.526], 25.3 -> 24.9 ms/token
- `jsb__Github_trivial__o78110`: 1.111x [0.914 .. 1.251], 30.7 -> 30.2 ms/token
- `jsb__JsonSchemaStore__project-1`: 1.112x [1.046 .. 1.157], 50.4 -> 46.2 ms/token
- `jsb__Kubernetes__kb_987_Normalized`: 1.650x [1.280 .. 1.969], 24.6 -> 14.0 ms/token
- `jsb__Glaiveai2K__calculate_retirement_savings_11a46a10`: 1.657x [1.421 .. 1.903], 19.0 -> 11.3 ms/token
- `jsb__JsonSchemaStore__cloudformation`: 1.662x [1.560 .. 1.868], 20.2 -> 11.9 ms/token
- `jsb__Glaiveai2K__calculate_area_f9afae07`: 1.695x [1.556 .. 1.793], 21.4 -> 13.1 ms/token
- `jsb__Github_trivial__o36645`: 1.742x [1.444 .. 1.955], 21.6 -> 11.8 ms/token
