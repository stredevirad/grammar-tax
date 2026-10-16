## Paired A/B: `pr28371c` vs `base` (ab-main)

206 grammars x 3 interleaved runs; bootstrap over grammars, 1000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / pr28371c) | **0.963x** | 0.953 .. 0.973 |
| median per-token cost, pr28371c / base | **1.014** (+1.4%) | 0.986 .. 1.050 |
| pooled p99, pr28371c / base | **1.076** (+7.6%) | 0.930 .. 1.077 |
| total replay time, pr28371c / base | 1.022 (+2.2%) | 1.012 .. 1.042 |

Per-grammar speedup: 59 faster, 147 slower; range 0.800x .. 1.255x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__JsonSchemaStore__samcli`: 0.800x [0.621 .. 0.933], 18.8 -> 21.2 ms/token
- `jsb__Kubernetes__kb_987_Normalized`: 0.817x [0.656 .. 0.975], 21.8 -> 25.6 ms/token
- `jsb__WashingtonPost__wp_42_Normalized`: 0.822x [0.641 .. 0.983], 37.6 -> 44.8 ms/token
- `jsb__Glaiveai2K__book_flight_c933065e`: 0.826x [0.793 .. 0.871], 30.1 -> 38.0 ms/token
- `jsb__WashingtonPost__wp_120_Normalized`: 0.832x [0.751 .. 1.002], 28.3 -> 32.2 ms/token
- `jsb__WashingtonPost__wp_6_Normalized`: 1.143x [1.060 .. 1.197], 20.1 -> 17.4 ms/token
- `jsb__Github_medium__o20530`: 1.148x [0.952 .. 1.311], 26.0 -> 22.7 ms/token
- `gbnf__english`: 1.170x [1.037 .. 1.297], 90.8 -> 81.4 ms/token
- `gbnf__chess`: 1.187x [0.985 .. 1.466], 25.6 -> 19.7 ms/token
- `gbnf__japanese`: 1.255x [1.119 .. 1.481], 35.6 -> 29.1 ms/token
