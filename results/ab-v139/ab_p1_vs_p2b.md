## Paired A/B: `p2b` vs `p1` (ab-v139)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1 / p2b) | **0.950x** | 0.940 .. 0.961 |
| median per-token cost, p2b / p1 | **1.063** (+6.3%) | 1.033 .. 1.100 |
| pooled p99, p2b / p1 | **0.927** (-7.3%) | 0.789 .. 1.056 |
| total replay time, p2b / p1 | 1.031 (+3.1%) | 1.011 .. 1.052 |

Per-grammar speedup: 50 faster, 156 slower; range 0.702x .. 1.238x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Snowplow__sp_70_Normalized`: 0.702x [0.627 .. 0.846], 10.8 -> 17.2 ms/token
- `jsb__WashingtonPost__wp_71_Normalized`: 0.759x [0.622 .. 0.849], 19.0 -> 23.0 ms/token
- `jsb__Github_trivial__o69498`: 0.761x [0.575 .. 1.124], 17.0 -> 24.9 ms/token
- `jsb__WashingtonPost__wp_70_Normalized`: 0.775x [0.655 .. 0.917], 19.1 -> 25.8 ms/token
- `jsb__Snowplow__sp_85_Normalized`: 0.786x [0.708 .. 0.879], 18.6 -> 23.4 ms/token
- `jsb__Kubernetes__kb_1079_Normalized`: 1.139x [1.048 .. 1.264], 16.5 -> 14.2 ms/token
- `jsb__Glaiveai2K__calculate_area_f9afae07`: 1.144x [1.032 .. 1.243], 14.9 -> 13.1 ms/token
- `jsb__JsonSchemaStore__mason-registry`: 1.174x [1.118 .. 1.289], 68.5 -> 60.6 ms/token
- `gbnf__japanese`: 1.188x [0.990 .. 1.507], 27.6 -> 22.6 ms/token
- `gbnf__chess`: 1.238x [0.927 .. 1.613], 16.5 -> 13.0 ms/token
