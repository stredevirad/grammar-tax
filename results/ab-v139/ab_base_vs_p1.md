## Paired A/B: `p1` vs `base` (ab-v139)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / p1) | **1.387x** | 1.368 .. 1.407 |
| median per-token cost, p1 / base | **0.721** (-27.9%) | 0.699 .. 0.734 |
| pooled p99, p1 / base | **0.882** (-11.8%) | 0.815 .. 0.971 |
| total replay time, p1 / base | 0.763 (-23.7%) | 0.740 .. 0.779 |

Per-grammar speedup: 206 faster, 0 slower; range 1.017x .. 1.911x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__english`: 1.017x [0.874 .. 1.332], 73.3 -> 79.6 ms/token
- `gbnf__japanese`: 1.038x [0.833 .. 1.377], 26.9 -> 27.6 ms/token
- `gbnf__c`: 1.074x [0.792 .. 1.298], 22.7 -> 18.9 ms/token
- `jsb__JsonSchemaStore__project-1`: 1.146x [1.033 .. 1.301], 50.4 -> 43.4 ms/token
- `jsb__JsonSchemaStore__mason-registry`: 1.166x [1.102 .. 1.206], 80.8 -> 68.5 ms/token
- `jsb__Github_easy__o60852`: 1.732x [1.670 .. 1.813], 20.1 -> 11.8 ms/token
- `jsb__Github_trivial__o36645`: 1.824x [1.653 .. 1.971], 21.6 -> 11.6 ms/token
- `jsb__Snowplow__sp_70_Normalized`: 1.834x [1.653 .. 2.023], 19.6 -> 10.8 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 1.839x [1.622 .. 2.057], 18.7 -> 10.0 ms/token
- `jsb__Snowplow__sp_43_Normalized`: 1.911x [1.773 .. 2.092], 21.2 -> 12.0 ms/token
