## Paired A/B: `reuse` vs `p1ascii` (ab-final)

206 grammars x 5 interleaved runs; bootstrap over grammars, 2000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1ascii / reuse) | **0.982x** | 0.978 .. 0.987 |
| median per-token cost, reuse / p1ascii | **1.012** (+1.2%) | 0.994 .. 1.039 |
| pooled p99, reuse / p1ascii | **1.069** (+6.9%) | 0.966 .. 1.107 |
| total replay time, reuse / p1ascii | 1.021 (+2.1%) | 1.009 .. 1.030 |

Per-grammar speedup: 67 faster, 139 slower; range 0.876x .. 1.085x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Github_easy__o6120`: 0.876x [0.761 .. 0.951], 14.4 -> 15.8 ms/token
- `jsb__JsonSchemaStore__samcli`: 0.891x [0.667 .. 1.023], 11.4 -> 11.8 ms/token
- `jsb__Github_hard__o61423`: 0.894x [0.740 .. 1.080], 11.2 -> 13.4 ms/token
- `jsb__Snowplow__sp_230_Normalized`: 0.903x [0.863 .. 0.947], 18.9 -> 20.2 ms/token
- `jsb__Glaiveai2K__search_news_eceea94e`: 0.914x [0.820 .. 1.070], 12.4 -> 13.7 ms/token
- `jsb__Github_ultra__o21220`: 1.059x [0.983 .. 1.181], 17.0 -> 15.7 ms/token
- `gbnf__english`: 1.059x [0.960 .. 1.115], 60.6 -> 58.3 ms/token
- `jsb__Github_ultra__o34345`: 1.067x [0.938 .. 1.130], 16.0 -> 15.3 ms/token
- `jsb__Github_ultra__o21135`: 1.079x [1.005 .. 1.145], 12.3 -> 11.4 ms/token
- `jsb__WashingtonPost__wp_70_Normalized`: 1.085x [0.891 .. 1.279], 21.0 -> 18.9 ms/token
