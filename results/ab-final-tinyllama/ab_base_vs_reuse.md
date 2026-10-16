## Paired A/B: `reuse` vs `base` (ab-final-tinyllama)

206 grammars x 3 interleaved runs; bootstrap over grammars, 2000 resamples, 95% CI.

| statistic | value | 95% CI |
|---|---|---|
| geometric-mean per-grammar speedup (base / reuse) | **1.258x** | 1.223 .. 1.298 |
| median per-token cost, reuse / base | **0.941** (-5.9%) | 0.873 .. 0.986 |
| pooled p99, reuse / base | **1.016** (+1.6%) | 0.942 .. 1.110 |
| total replay time, reuse / base | 0.966 (-3.4%) | 0.922 .. 1.005 |

Per-grammar speedup: 184 faster, 22 slower; range 0.905x .. 2.132x. Slowest 5 / fastest 5 (geomean over runs [min .. max]):

- `jsb__Github_trivial__o10076`: 0.905x [0.874 .. 0.936], 5.9 -> 6.8 ms/token
- `jsb__JsonSchemaStore__cluster`: 0.912x [0.718 .. 1.285], 6.4 -> 7.8 ms/token
- `jsb__JsonSchemaStore__inventory`: 0.914x [0.896 .. 0.948], 6.2 -> 6.8 ms/token
- `jsb__Glaiveai2K__search_news_eceea94e`: 0.916x [0.765 .. 1.276], 6.5 -> 8.0 ms/token
- `jsb__WashingtonPost__wp_61_Normalized`: 0.935x [0.733 .. 1.344], 6.3 -> 7.7 ms/token
- `jsb__Github_trivial__o45630`: 2.030x [1.898 .. 2.162], 2.6 -> 1.3 ms/token
- `jsb__JsonSchemaStore__json-patch`: 2.046x [1.983 .. 2.130], 2.6 -> 1.3 ms/token
- `jsb__Github_easy__o39431`: 2.059x [1.755 .. 2.260], 4.3 -> 1.9 ms/token
- `jsb__Github_trivial__o58627`: 2.105x [1.697 .. 2.754], 2.5 -> 1.3 ms/token
- `jsb__Snowplow__sp_218_Normalized`: 2.132x [1.762 .. 2.995], 2.7 -> 1.4 ms/token
