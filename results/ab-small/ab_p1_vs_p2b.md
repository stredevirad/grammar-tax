## Paired A/B: `p2b` vs `p1` (ab-small-r12)

206 grammars x 2 interleaved runs; bootstrap over grammars, 500 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (p1 / p2b) | **0.965x** | 0.955 .. 0.974 |
| median per-token cost, p2b / p1 | **1.044** (+4.4%) | 1.023 .. 1.066 |
| pooled p99, p2b / p1 | **0.952** (-4.8%) | 0.823 .. 1.097 |
| total replay time, p2b / p1 | 1.043 (+4.3%) | 1.030 .. 1.054 |

Per-grammar speedup: 56 faster, 150 slower; range 0.795x .. 1.349x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Github_trivial__o7488`: 0.795x [0.612 .. 1.033], 17.2 -> 21.6 ms/token
- `jsb__Github_trivial__o75595`: 0.820x [0.685 .. 0.981], 11.8 -> 14.5 ms/token
- `jsb__Github_easy__o17401`: 0.842x [0.783 .. 0.905], 22.1 -> 26.4 ms/token
- `jsb__Github_easy__o23088`: 0.850x [0.759 .. 0.952], 16.2 -> 19.2 ms/token
- `jsb__WashingtonPost__wp_70_Normalized`: 0.852x [0.795 .. 0.912], 19.7 -> 23.2 ms/token
- `jsb__Github_trivial__o83692`: 1.144x [1.093 .. 1.198], 15.6 -> 13.7 ms/token
- `jsb__Snowplow__sp_218_Normalized`: 1.154x [1.109 .. 1.202], 13.1 -> 11.3 ms/token
- `jsb__Kubernetes__kb_943_Normalized`: 1.163x [1.158 .. 1.167], 14.6 -> 12.6 ms/token
- `gbnf__arithmetic`: 1.176x [1.098 .. 1.260], 24.4 -> 20.7 ms/token
- `gbnf__chess`: 1.349x [1.168 .. 1.558], 16.7 -> 12.5 ms/token
