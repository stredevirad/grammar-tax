# Callgrind instruction counts

Collection limited to llama_grammar_apply_impl + llama_grammar_accept_impl, first 40 tokens of each trace, median9 (9 grammars) and worst10 (10 grammars) sets, GGML_NATIVE=OFF build (D1.7). Deterministic up to tiny malloc-path differences (<0.001%).

| variant | median9 instructions | vs base | worst10 instructions | vs base |
|---|---|---|---|---|
| baseline | 60.75 G | +0.0% | 193.12 G | +0.0% |
| 0001 | 38.98 G | -35.8% | 168.94 G | -12.5% |
| 0001+0002 | 32.14 G | -47.1% | 118.42 G | -38.7% |
| final 0001+0002+0003 | 32.13 G | -47.1% | 118.35 G | -38.7% |
| 0001+P2 (rej.) | 41.09 G | -32.4% | 180.48 G | -6.5% |
| 0001+P2b (rej.) | 38.63 G | -36.4% | 142.92 G | -26.0% |
| 0001+skip (rej.) | 38.65 G | -36.4% | 165.44 G | -14.3% |
| 0001+eog (rej.) | 37.79 G | -37.8% | 167.62 G | -13.2% |
| #28371 c29085b | 60.84 G | +0.1% | 193.25 G | +0.1% |
| 0001+P2+#28371 | 41.17 G | -32.2% | 180.56 G | -6.5% |
