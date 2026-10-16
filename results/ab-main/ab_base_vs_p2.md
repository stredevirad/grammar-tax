## Paired A/B: `p2` vs `base` (ab-main)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / p2) | **1.423x** | 1.402 .. 1.445 |
| median per-token cost, p2 / base | **0.704** (-29.6%) | 0.685 .. 0.729 |
| pooled p99, p2 / base | **0.788** (-21.2%) | 0.637 .. 0.931 |
| total replay time, p2 / base | 0.750 (-25.0%) | 0.733 .. 0.766 |

Per-grammar speedup: 206 faster, 0 slower; range 1.119x .. 1.964x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__japanese`: 1.119x [1.055 .. 1.189], 35.6 -> 33.8 ms/token
- `jsb__Snowplow__sp_392_Normalized`: 1.151x [1.095 .. 1.188], 43.0 -> 36.8 ms/token
- `jsb__Github_easy__o12624`: 1.163x [1.087 .. 1.248], 32.5 -> 28.0 ms/token
- `jsb__Github_trivial__o85930`: 1.174x [1.092 .. 1.246], 41.8 -> 38.0 ms/token
- `jsb__Snowplow__sp_408_Normalized`: 1.176x [1.100 .. 1.270], 67.4 -> 58.0 ms/token
- `jsb__Github_hard__o41300`: 1.815x [1.609 .. 2.025], 21.2 -> 12.6 ms/token
- `jsb__Github_trivial__o21828`: 1.856x [1.704 .. 2.169], 20.5 -> 11.3 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 1.892x [1.479 .. 2.227], 20.1 -> 9.4 ms/token
- `jsb__Github_medium__o20530`: 1.901x [1.766 .. 1.983], 26.0 -> 13.1 ms/token
- `jsb__Kubernetes__kb_943_Normalized`: 1.964x [1.772 .. 2.211], 26.1 -> 13.5 ms/token
