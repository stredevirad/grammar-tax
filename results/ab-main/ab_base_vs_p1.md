## Paired A/B: `p1` vs `base` (ab-main)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / p1) | **1.458x** | 1.441 .. 1.477 |
| median per-token cost, p1 / base | **0.684** (-31.6%) | 0.668 .. 0.701 |
| pooled p99, p1 / base | **0.785** (-21.5%) | 0.722 .. 0.854 |
| total replay time, p1 / base | 0.721 (-27.9%) | 0.702 .. 0.735 |

Per-grammar speedup: 206 faster, 0 slower; range 1.118x .. 1.934x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__arithmetic`: 1.118x [1.052 .. 1.249], 31.3 -> 28.4 ms/token
- `gbnf__english`: 1.120x [0.985 .. 1.196], 90.8 -> 88.5 ms/token
- `gbnf__c`: 1.217x [1.011 .. 1.531], 27.3 -> 23.4 ms/token
- `jsb__WashingtonPost__wp_42_Normalized`: 1.230x [1.017 .. 1.362], 37.6 -> 27.7 ms/token
- `jsb__Github_trivial__o85930`: 1.236x [1.051 .. 1.385], 41.8 -> 33.2 ms/token
- `jsb__Github_trivial__o21828`: 1.791x [1.531 .. 2.246], 20.5 -> 11.0 ms/token
- `jsb__Snowplow__sp_218_Normalized`: 1.801x [1.666 .. 2.058], 18.9 -> 11.4 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 1.829x [1.573 .. 2.061], 20.1 -> 9.8 ms/token
- `jsb__Kubernetes__kb_943_Normalized`: 1.887x [1.840 .. 1.937], 26.1 -> 13.5 ms/token
- `jsb__JsonSchemaStore__cloudformation`: 1.934x [1.461 .. 2.299], 24.4 -> 11.5 ms/token
