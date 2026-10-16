## Paired A/B: `p1skip` vs `p1` (ab-small-r12)

206 grammars x 2 interleaved runs; bootstrap over grammars, 500 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1 / p1skip) | **0.999x** | 0.990 .. 1.008 |
| median per-token cost, p1skip / p1 | **1.003** (+0.3%) | 0.980 .. 1.026 |
| pooled p99, p1skip / p1 | **1.076** (+7.6%) | 0.916 .. 1.148 |
| total replay time, p1skip / p1 | 1.013 (+1.3%) | 0.996 .. 1.027 |

Per-grammar speedup: 103 faster, 103 slower; range 0.803x .. 1.210x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Kubernetes__kb_1037_Normalized`: 0.803x [0.696 .. 0.925], 16.8 -> 21.2 ms/token
- `jsb__Github_hard__o47670`: 0.851x [0.773 .. 0.936], 12.8 -> 15.2 ms/token
- `jsb__JsonSchemaStore__minecraft-dimension`: 0.856x [0.852 .. 0.861], 12.2 -> 14.3 ms/token
- `jsb__Snowplow__sp_366_Normalized`: 0.861x [0.836 .. 0.886], 16.3 -> 19.0 ms/token
- `jsb__JsonSchemaStore__sarif-2`: 0.861x [0.855 .. 0.867], 17.5 -> 20.3 ms/token
- `gbnf__japanese`: 1.171x [1.074 .. 1.277], 27.3 -> 23.4 ms/token
- `jsb__Snowplow__sp_218_Normalized`: 1.176x [1.089 .. 1.271], 13.1 -> 11.1 ms/token
- `gbnf__english`: 1.191x [1.055 .. 1.345], 79.4 -> 67.1 ms/token
- `jsb__Github_trivial__o46562`: 1.197x [1.030 .. 1.390], 13.2 -> 10.8 ms/token
- `gbnf__arithmetic`: 1.210x [1.025 .. 1.427], 24.4 -> 20.2 ms/token
