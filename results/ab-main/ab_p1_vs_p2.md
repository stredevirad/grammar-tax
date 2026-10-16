## Paired A/B: `p2` vs `p1` (ab-main)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1 / p2) | **0.976x** | 0.967 .. 0.986 |
| median per-token cost, p2 / p1 | **1.022** (+2.2%) | 1.001 .. 1.064 |
| pooled p99, p2 / p1 | **0.994** (-0.6%) | 0.853 .. 1.195 |
| total replay time, p2 / p1 | 1.040 (+4.0%) | 1.023 .. 1.058 |

Per-grammar speedup: 65 faster, 141 slower; range 0.830x .. 1.380x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Snowplow__sp_301_Normalized`: 0.830x [0.756 .. 0.879], 19.7 -> 26.1 ms/token
- `jsb__WashingtonPost__wp_90_Normalized`: 0.832x [0.706 .. 0.920], 20.7 -> 23.2 ms/token
- `jsb__Glaiveai2K__calculate_area_8e760c24`: 0.844x [0.801 .. 0.887], 14.2 -> 16.8 ms/token
- `jsb__Github_hard__o6347`: 0.859x [0.798 .. 0.901], 47.7 -> 59.4 ms/token
- `jsb__Snowplow__sp_218_Normalized`: 0.863x [0.727 .. 1.008], 11.4 -> 12.3 ms/token
- `jsb__JsonSchemaStore__samcli`: 1.180x [0.971 .. 1.504], 14.3 -> 11.5 ms/token
- `jsb__Github_medium__o20530`: 1.191x [1.157 .. 1.217], 16.0 -> 13.1 ms/token
- `gbnf__arithmetic`: 1.200x [1.124 .. 1.243], 28.4 -> 22.9 ms/token
- `jsb__Github_trivial__o83692`: 1.225x [0.951 .. 1.437], 18.0 -> 13.3 ms/token
- `gbnf__chess`: 1.380x [1.326 .. 1.421], 19.1 -> 13.7 ms/token
