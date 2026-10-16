## Paired A/B: `reuse` vs `base` (ab-final)

206 grammars x 5 interleaved runs; bootstrap over grammars, 2000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / reuse) | **1.470x** | 1.455 .. 1.486 |
| median per-token cost, reuse / base | **0.673** (-32.7%) | 0.660 .. 0.688 |
| pooled p99, reuse / base | **0.821** (-17.9%) | 0.755 .. 0.852 |
| total replay time, reuse / base | 0.712 (-28.8%) | 0.696 .. 0.730 |

Per-grammar speedup: 206 faster, 0 slower; range 1.033x .. 1.886x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__japanese`: 1.033x [0.918 .. 1.227], 28.0 -> 27.9 ms/token
- `jsb__Github_hard__o6347`: 1.217x [1.090 .. 1.345], 60.8 -> 52.6 ms/token
- `gbnf__arithmetic`: 1.234x [1.155 .. 1.332], 30.3 -> 24.6 ms/token
- `jsb__Github_ultra__o46322`: 1.240x [1.118 .. 1.416], 214.8 -> 173.0 ms/token
- `jsb__JsonSchemaStore__rustfmt`: 1.245x [1.146 .. 1.365], 103.4 -> 84.7 ms/token
- `jsb__Kubernetes__kb_431_Normalized`: 1.728x [1.476 .. 1.895], 20.7 -> 11.9 ms/token
- `jsb__Glaiveai2K__calculate_retirement_savings_11a46a10`: 1.762x [1.355 .. 2.183], 20.5 -> 11.3 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 1.783x [1.501 .. 2.077], 18.7 -> 10.4 ms/token
- `jsb__Glaiveai2K__convert_temperature_cc92bc87`: 1.810x [1.543 .. 2.385], 19.5 -> 11.0 ms/token
- `jsb__Github_trivial__o21828`: 1.886x [1.562 .. 2.203], 17.1 -> 9.1 ms/token
