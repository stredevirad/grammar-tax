## Paired A/B: `p2b` vs `p1ascii` (ab-v139)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1ascii / p2b) | **0.920x** | 0.909 .. 0.930 |
| median per-token cost, p2b / p1ascii | **1.136** (+13.6%) | 1.077 .. 1.160 |
| pooled p99, p2b / p1ascii | **0.968** (-3.2%) | 0.784 .. 1.170 |
| total replay time, p2b / p1ascii | 1.086 (+8.6%) | 1.061 .. 1.113 |

Per-grammar speedup: 38 faster, 168 slower; range 0.733x .. 1.235x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Snowplow__sp_85_Normalized`: 0.733x [0.648 .. 0.816], 17.4 -> 23.4 ms/token
- `jsb__Snowplow__sp_70_Normalized`: 0.735x [0.622 .. 0.825], 10.7 -> 17.2 ms/token
- `jsb__WashingtonPost__wp_88_Normalized`: 0.749x [0.693 .. 0.787], 22.5 -> 29.9 ms/token
- `gbnf__list`: 0.752x [0.688 .. 0.809], 22.3 -> 29.2 ms/token
- `jsb__WashingtonPost__wp_90_Normalized`: 0.768x [0.736 .. 0.813], 18.4 -> 24.4 ms/token
- `jsb__JsonSchemaStore__mason-registry`: 1.123x [1.086 .. 1.170], 65.8 -> 60.6 ms/token
- `jsb__Github_easy__o30305`: 1.129x [1.038 .. 1.223], 23.0 -> 20.3 ms/token
- `gbnf__arithmetic`: 1.136x [0.886 .. 1.346], 24.6 -> 21.0 ms/token
- `gbnf__chess`: 1.165x [0.970 .. 1.304], 15.5 -> 13.0 ms/token
- `gbnf__japanese`: 1.235x [1.163 .. 1.365], 29.1 -> 22.6 ms/token
