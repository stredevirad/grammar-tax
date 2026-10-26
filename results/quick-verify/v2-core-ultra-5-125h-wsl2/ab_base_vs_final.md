## Paired A/B: `final` vs `base` (raw)

20 grammars x 3 interleaved runs; bootstrap over grammars, 2000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / final) | **1.590x** | 1.556 .. 1.626 |
| median per-token cost, final / base | **0.672** (-32.8%) | 0.601 .. 0.692 |
| pooled p99, final / base | **0.576** (-42.4%) | 0.562 .. 0.711 |
| total replay time, final / base | 0.589 (-41.1%) | 0.574 .. 0.646 |

Per-grammar speedup: 20 faster, 0 slower; range 1.456x .. 1.735x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Snowplow__sp_286_Normalized`: 1.456x [1.393 .. 1.548], 17.7 -> 11.5 ms/token
- `jsb__Kubernetes__kb_373_Normalized`: 1.479x [1.335 .. 1.616], 15.6 -> 11.1 ms/token
- `jsb__Github_easy__o65361`: 1.518x [1.452 .. 1.586], 20.5 -> 13.5 ms/token
- `jsb__Github_easy__o53019`: 1.528x [1.496 .. 1.591], 20.8 -> 13.3 ms/token
- `jsb__Kubernetes__kb_983_Normalized`: 1.530x [1.412 .. 1.668], 17.0 -> 10.3 ms/token
- `jsb__Glaiveai2K__calculate_retirement_savings_11a46a10`: 1.686x [1.599 .. 1.824], 13.2 -> 8.0 ms/token
- `jsb__Glaiveai2K__calculate_area_8cdcc04b`: 1.692x [1.606 .. 1.803], 16.0 -> 8.9 ms/token
- `jsb__WashingtonPost__wp_120_Normalized`: 1.722x [1.443 .. 2.128], 22.3 -> 12.3 ms/token
- `jsb__Glaiveai2K__calculate_area_f5284246`: 1.734x [1.692 .. 1.801], 17.8 -> 10.3 ms/token
- `jsb__Github_ultra__o46322`: 1.735x [1.667 .. 1.777], 233.0 -> 131.1 ms/token
