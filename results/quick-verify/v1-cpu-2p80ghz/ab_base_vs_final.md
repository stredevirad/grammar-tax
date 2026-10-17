## Paired A/B: `final` vs `base` (raw)

20 grammars x 3 interleaved runs; bootstrap over grammars, 2000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / final) | **1.566x** | 1.496 .. 1.633 |
| median per-token cost, final / base | **0.632** (-36.8%) | 0.593 .. 0.677 |
| pooled p99, final / base | **0.789** (-21.1%) | 0.642 .. 0.830 |
| total replay time, final / base | 0.686 (-31.4%) | 0.628 .. 0.730 |

Per-grammar speedup: 20 faster, 0 slower; range 1.333x .. 1.859x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Github_ultra__o46322`: 1.333x [1.302 .. 1.391], 274.7 -> 204.7 ms/token
- `jsb__Github_hard__o6347`: 1.333x [1.306 .. 1.374], 78.3 -> 58.8 ms/token
- `gbnf__japanese`: 1.355x [1.224 .. 1.450], 43.0 -> 31.0 ms/token
- `jsb__Github_trivial__o45026`: 1.424x [1.376 .. 1.455], 42.9 -> 29.6 ms/token
- `jsb__Github_easy__o30305`: 1.438x [1.235 .. 1.561], 35.2 -> 23.3 ms/token
- `jsb__Github_trivial__o46562`: 1.691x [1.609 .. 1.858], 21.2 -> 12.8 ms/token
- `jsb__JsonSchemaStore__jsbeautifyrc`: 1.718x [1.636 .. 1.849], 29.8 -> 17.5 ms/token
- `jsb__Glaiveai2K__calculate_area_82427f82`: 1.797x [1.604 .. 1.936], 25.9 -> 13.9 ms/token
- `jsb__Github_hard__o58761`: 1.801x [1.765 .. 1.859], 28.7 -> 15.8 ms/token
- `jsb__Glaiveai2K__calculate_area_f9afae07`: 1.859x [1.554 .. 2.052], 28.0 -> 14.4 ms/token
