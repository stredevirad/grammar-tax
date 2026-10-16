## End to end: `reuse` vs `base` (2 interleaved rounds, Qwen2.5-0.5B Q4_K_M, 4 threads)

| mode | grammars | tokens/s A (pooled, median over rounds) | tokens/s B | B / A pooled | geomean per-grammar B / A | grammar ms/token A -> B | token sequences identical |
|---|---|---|---|---|---|---|---|
| resample | 28 | 34.89 | 35.42 | 1.015 | 1.031 [1.012 .. 1.050] | 1.39 -> 0.96 | yes |
| first | 28 | 15.73 | 19.54 | 1.242 | 1.239 [1.223 .. 1.256] | 34.54 -> 22.68 | yes |
