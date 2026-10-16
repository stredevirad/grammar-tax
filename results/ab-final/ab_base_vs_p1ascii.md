## Paired A/B: `p1ascii` vs `base` (ab-final)

206 grammars x 5 interleaved runs; bootstrap over grammars, 2000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / p1ascii) | **1.497x** | 1.480 .. 1.514 |
| median per-token cost, p1ascii / base | **0.676** (-32.4%) | 0.650 .. 0.685 |
| pooled p99, p1ascii / base | **0.772** (-22.8%) | 0.714 .. 0.820 |
| total replay time, p1ascii / base | 0.696 (-30.4%) | 0.682 .. 0.712 |

Per-grammar speedup: 206 faster, 0 slower; range 1.021x .. 1.878x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `gbnf__japanese`: 1.021x [0.908 .. 1.126], 28.0 -> 26.5 ms/token
- `jsb__Github_ultra__o46322`: 1.224x [1.166 .. 1.267], 214.8 -> 174.1 ms/token
- `jsb__Github_hard__o6347`: 1.232x [1.135 .. 1.317], 60.8 -> 49.3 ms/token
- `gbnf__english`: 1.254x [1.153 .. 1.503], 74.9 -> 60.6 ms/token
- `jsb__JsonSchemaStore__mason-registry`: 1.298x [1.140 .. 1.461], 87.1 -> 63.9 ms/token
- `jsb__Kubernetes__kb_431_Normalized`: 1.783x [1.697 .. 1.851], 20.7 -> 11.5 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 1.795x [1.620 .. 1.932], 18.7 -> 10.2 ms/token
- `jsb__Glaiveai2K__convert_temperature_cc92bc87`: 1.813x [1.683 .. 2.230], 19.5 -> 11.2 ms/token
- `jsb__Github_trivial__o7488`: 1.818x [1.404 .. 2.117], 32.0 -> 15.3 ms/token
- `jsb__Github_trivial__o21828`: 1.878x [1.686 .. 2.125], 17.1 -> 9.2 ms/token
