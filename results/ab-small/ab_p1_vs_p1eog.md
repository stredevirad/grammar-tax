## Paired A/B: `p1eog` vs `p1` (ab-small-r12)

206 grammars x 2 interleaved runs; bootstrap over grammars, 500 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1 / p1eog) | **1.002x** | 0.993 .. 1.012 |
| median per-token cost, p1eog / p1 | **1.009** (+0.9%) | 0.981 .. 1.039 |
| pooled p99, p1eog / p1 | **1.037** (+3.7%) | 0.959 .. 1.107 |
| total replay time, p1eog / p1 | 1.011 (+1.1%) | 1.002 .. 1.021 |

Per-grammar speedup: 100 faster, 106 slower; range 0.814x .. 1.314x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__WashingtonPost__wp_61_Normalized`: 0.814x [0.748 .. 0.887], 28.1 -> 34.8 ms/token
- `jsb__WashingtonPost__wp_67_Normalized`: 0.855x [0.845 .. 0.865], 26.5 -> 31.0 ms/token
- `jsb__Glaiveai2K__calculate_area_b5e19bd6`: 0.875x [0.761 .. 1.005], 14.4 -> 16.6 ms/token
- `jsb__Github_easy__o12214`: 0.876x [0.854 .. 0.898], 18.3 -> 20.9 ms/token
- `jsb__JsonSchemaStore__minecraft-dimension`: 0.878x [0.795 .. 0.970], 12.2 -> 14.0 ms/token
- `jsb__Github_ultra__o21137`: 1.131x [1.059 .. 1.209], 12.4 -> 11.0 ms/token
- `gbnf__arithmetic`: 1.142x [0.971 .. 1.344], 24.4 -> 21.4 ms/token
- `gbnf__c`: 1.215x [1.176 .. 1.256], 19.6 -> 16.1 ms/token
- `jsb__WashingtonPost__wp_109_Normalized`: 1.217x [1.040 .. 1.424], 17.0 -> 13.8 ms/token
- `jsb__Snowplow__sp_218_Normalized`: 1.314x [1.224 .. 1.411], 13.1 -> 9.9 ms/token
