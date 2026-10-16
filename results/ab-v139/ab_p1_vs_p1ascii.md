## Paired A/B: `p1ascii` vs `p1` (ab-v139)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1 / p1ascii) | **1.033x** | 1.022 .. 1.045 |
| median per-token cost, p1ascii / p1 | **0.959** (-4.1%) | 0.933 .. 0.996 |
| pooled p99, p1ascii / p1 | **0.930** (-7.0%) | 0.859 .. 1.015 |
| total replay time, p1ascii / p1 | 0.958 (-4.2%) | 0.937 .. 0.981 |

Per-grammar speedup: 142 faster, 64 slower; range 0.829x .. 1.317x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Github_easy__o30305`: 0.829x [0.777 .. 0.902], 18.7 -> 23.0 ms/token
- `jsb__Github_medium__o48419`: 0.845x [0.791 .. 0.892], 14.5 -> 16.9 ms/token
- `jsb__Github_easy__o60852`: 0.861x [0.850 .. 0.883], 11.8 -> 13.4 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 0.866x [0.678 .. 1.052], 10.0 -> 11.0 ms/token
- `jsb__WashingtonPost__wp_70_Normalized`: 0.883x [0.745 .. 1.026], 19.1 -> 21.3 ms/token
- `jsb__WashingtonPost__wp_115_Normalized`: 1.197x [1.081 .. 1.448], 20.4 -> 18.8 ms/token
- `jsb__Kubernetes__kb_943_Normalized`: 1.231x [1.146 .. 1.320], 16.5 -> 14.4 ms/token
- `gbnf__english`: 1.234x [1.182 .. 1.262], 79.6 -> 63.1 ms/token
- `jsb__Snowplow__sp_362_Normalized`: 1.241x [1.187 .. 1.326], 17.6 -> 14.5 ms/token
- `jsb__WashingtonPost__wp_90_Normalized`: 1.317x [1.190 .. 1.421], 24.4 -> 18.4 ms/token
