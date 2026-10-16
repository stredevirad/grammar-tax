## Paired A/B: `p1ascii` vs `p1` (ab-small-r12)

206 grammars x 2 interleaved runs; bootstrap over grammars, 500 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1 / p1ascii) | **1.036x** | 1.028 .. 1.046 |
| median per-token cost, p1ascii / p1 | **0.975** (-2.5%) | 0.949 .. 0.994 |
| pooled p99, p1ascii / p1 | **0.979** (-2.1%) | 0.817 .. 1.089 |
| total replay time, p1ascii / p1 | 0.969 (-3.1%) | 0.954 .. 0.982 |

Per-grammar speedup: 151 faster, 55 slower; range 0.849x .. 1.430x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__JsonSchemaStore__minecraft-dimension`: 0.849x [0.822 .. 0.876], 12.2 -> 14.4 ms/token
- `jsb__Github_ultra__o54621`: 0.871x [0.866 .. 0.876], 29.2 -> 33.5 ms/token
- `jsb__Snowplow__sp_400_Normalized`: 0.883x [0.843 .. 0.925], 15.4 -> 17.5 ms/token
- `jsb__Github_easy__o60852`: 0.902x [0.828 .. 0.983], 11.7 -> 12.9 ms/token
- `jsb__Glaiveai2K__calculate_area_b5e19bd6`: 0.904x [0.891 .. 0.917], 14.4 -> 15.9 ms/token
- `jsb__Kubernetes__kb_983_Normalized`: 1.189x [1.060 .. 1.334], 16.9 -> 14.1 ms/token
- `gbnf__arithmetic`: 1.191x [1.169 .. 1.214], 24.4 -> 20.5 ms/token
- `jsb__Kubernetes__kb_987_Normalized`: 1.207x [1.104 .. 1.319], 15.6 -> 12.8 ms/token
- `jsb__WashingtonPost__wp_42_Normalized`: 1.335x [1.293 .. 1.378], 32.0 -> 24.0 ms/token
- `gbnf__english`: 1.430x [1.285 .. 1.592], 79.4 -> 55.8 ms/token
