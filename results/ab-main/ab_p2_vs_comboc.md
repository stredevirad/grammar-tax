## Paired A/B: `comboc` vs `p2` (ab-main)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p2 / comboc) | **0.975x** | 0.967 .. 0.982 |
| median per-token cost, comboc / p2 | **1.008** (+0.8%) | 0.973 .. 1.048 |
| pooled p99, comboc / p2 | **1.044** (+4.4%) | 0.952 .. 1.136 |
| total replay time, comboc / p2 | 1.005 (+0.5%) | 0.988 .. 1.021 |

Per-grammar speedup: 64 faster, 142 slower; range 0.810x .. 1.176x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Snowplow__sp_43_Normalized`: 0.810x [0.749 .. 0.902], 13.2 -> 14.6 ms/token
- `jsb__Github_trivial__o47165`: 0.830x [0.728 .. 0.950], 25.8 -> 28.2 ms/token
- `jsb__Kubernetes__kb_143_Normalized`: 0.837x [0.603 .. 1.021], 13.2 -> 14.8 ms/token
- `jsb__Github_trivial__o21828`: 0.841x [0.633 .. 1.183], 11.3 -> 15.1 ms/token
- `jsb__JsonSchemaStore__json-patch`: 0.853x [0.703 .. 0.993], 21.9 -> 24.6 ms/token
- `jsb__Glaiveai2K__calculate_area_82427f82`: 1.095x [0.855 .. 1.352], 15.3 -> 13.5 ms/token
- `jsb__Github_trivial__o10076`: 1.119x [0.968 .. 1.211], 27.3 -> 23.6 ms/token
- `jsb__Snowplow__sp_70_Normalized`: 1.135x [0.923 .. 1.619], 12.4 -> 12.6 ms/token
- `gbnf__japanese`: 1.172x [0.957 .. 1.531], 33.8 -> 30.7 ms/token
- `jsb__Glaiveai2K__calculate_area_8e760c24`: 1.176x [0.873 .. 1.420], 16.8 -> 12.8 ms/token
