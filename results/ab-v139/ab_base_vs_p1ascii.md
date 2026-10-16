## Paired A/B: `p1ascii` vs `base` (ab-v139)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / p1ascii) | **1.433x** | 1.415 .. 1.453 |
| median per-token cost, p1ascii / base | **0.675** (-32.5%) | 0.663 .. 0.710 |
| pooled p99, p1ascii / base | **0.757** (-24.3%) | 0.733 .. 0.897 |
| total replay time, p1ascii / base | 0.730 (-27.0%) | 0.711 .. 0.750 |

Per-grammar speedup: 205 faster, 1 slower; range 0.999x .. 1.982x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__japanese`: 0.999x [0.919 .. 1.172], 26.9 -> 29.1 ms/token
- `gbnf__c`: 1.044x [0.932 .. 1.274], 22.7 -> 22.4 ms/token
- `jsb__Github_trivial__o47165`: 1.156x [1.017 .. 1.465], 30.2 -> 26.4 ms/token
- `gbnf__arithmetic`: 1.181x [1.122 .. 1.267], 28.5 -> 24.6 ms/token
- `jsb__Github_hard__o6347`: 1.185x [1.069 .. 1.389], 65.0 -> 53.5 ms/token
- `jsb__Snowplow__sp_70_Normalized`: 1.749x [1.304 .. 2.040], 19.6 -> 10.7 ms/token
- `jsb__Kubernetes__kb_987_Normalized`: 1.762x [1.413 .. 2.087], 24.6 -> 13.3 ms/token
- `jsb__JsonSchemaStore__minecraft-recipe`: 1.763x [1.557 .. 1.989], 22.7 -> 12.9 ms/token
- `jsb__Snowplow__sp_218_Normalized`: 1.812x [1.518 .. 2.441], 16.6 -> 10.7 ms/token
- `jsb__Snowplow__sp_43_Normalized`: 1.982x [1.624 .. 2.568], 21.2 -> 11.3 ms/token
