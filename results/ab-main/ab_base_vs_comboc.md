## Paired A/B: `comboc` vs `base` (ab-main)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / comboc) | **1.387x** | 1.367 .. 1.408 |
| median per-token cost, comboc / base | **0.710** (-29.0%) | 0.681 .. 0.740 |
| pooled p99, comboc / base | **0.823** (-17.7%) | 0.675 .. 0.995 |
| total replay time, comboc / base | 0.753 (-24.7%) | 0.731 .. 0.772 |

Per-grammar speedup: 206 faster, 0 slower; range 1.116x .. 1.976x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Snowplow__sp_408_Normalized`: 1.116x [1.010 .. 1.238], 67.4 -> 56.6 ms/token
- `jsb__JsonSchemaStore__project-1`: 1.146x [0.955 .. 1.467], 57.1 -> 53.2 ms/token
- `jsb__Github_ultra__o46322`: 1.153x [1.014 .. 1.259], 242.7 -> 210.0 ms/token
- `jsb__Github_trivial__o47165`: 1.153x [0.990 .. 1.553], 35.1 -> 28.2 ms/token
- `jsb__Github_ultra__o17072`: 1.157x [1.052 .. 1.254], 50.7 -> 43.5 ms/token
- `jsb__Glaiveai2K__calculate_area_06b6879e`: 1.748x [1.628 .. 1.908], 25.6 -> 14.3 ms/token
- `jsb__JsonSchemaStore__cloudformation`: 1.775x [1.230 .. 2.203], 24.4 -> 12.0 ms/token
- `jsb__JsonSchemaStore__minecraft-recipe`: 1.777x [1.699 .. 1.904], 25.3 -> 13.8 ms/token
- `jsb__Kubernetes__kb_943_Normalized`: 1.789x [1.579 .. 2.095], 26.1 -> 15.1 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 1.976x [1.776 .. 2.298], 20.1 -> 10.4 ms/token
