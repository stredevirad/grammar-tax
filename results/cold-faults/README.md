# Cold-process page faults (single runs)

Grammar `jsb__Github_ultra__o46322` (600 live stacks at peak) replayed with the Qwen2.5 vocab in a **fresh
`grammar_replay` process per row**, `perf stat -e page-faults` over the whole process (vocab load included).
Cases: the grammar alone; alone with `GLIBC_TUNABLES=glibc.malloc.mmap_threshold=67108864:glibc.malloc.trim_threshold=268435456`;
and after `gbnf__json` in the same process (the warm case of the A/B runs). These are **single runs, not a
paired A/B**; the paired cold A/B is in `results/cold-final/` and `results/cold-reuse/` (`scripts/cold_ab.sh`).
Mechanism and decisions: DECISIONS.md D3.13, D3.15.

## 2026-10-16, study host ("Xeon @ 2.80GHz"), kernel fc-v139: the numbers quoted in the docs

The raw `perf stat` files of these runs were overwritten (each run reused `build/pf.txt`); the values are copied
from the session log, with the command that produced them. Only the last row's raw output survived.

| time (UTC) | build | case | ms / token | page faults |
|---|---|---|---|---|
| 14:36 | base | alone | 282.8 | 3,410,174 |
| 14:36 | base | after json | 227.2 | 87,398 |
| 14:36 | 0001 | alone | 415.9 | 20,890,538 |
| 14:36 | 0001 | after json | 196.3 | 43,933 |
| 14:57 | base | alone | 235.3 | 3,410,174 |
| 14:57 | base | alone, tunables | 213.2 | 26,458 |
| 14:57 | 0001+0002 | alone | 383.4 | 20,890,537 |
| 14:57 | 0001+0002 | alone, tunables | 162.5 | 17,908 |
| 15:15 | 0001+0002 | alone | 377.6 | 20,890,537 |
| 15:15 | 0001+0002+0003 (final) | alone | 180.3 | 595,833 |

## 2026-10-17 re-run, new host ("Xeon @ 2.10GHz", model 207), same kernel: `rerun-2026-10-17/`

`scripts/cold_faults.sh results/cold-faults/rerun-2026-10-17 build/bench-base build/bench-p1 build/bench-p1ascii build/bench-reuse`
(same binaries as the study; raw `perf stat` output and per-grammar JSON kept).

| build | alone: ms / token | alone: faults | tunables: ms / token | tunables: faults | after json: ms / token | after json: faults |
|---|---|---|---|---|---|---|
| base | 215.1 | 3,410,178 | 189.0 | 24,862 | 189.4 | 69,930 |
| 0001 | 290.0 | 20,890,554 | 155.8 | 17,908 | 150.8 | 43,934 |
| 0001+0002 | 261.3 | 20,888,157 | 140.8 | 17,907 | 143.1 | 43,938 |
| final (0001+0002+0003) | 143.4 | 595,889 | 141.3 | 17,904 | 144.8 | 43,364 |

- The page-fault counts reproduce the logged ones closely (they depend on the allocation sequence, not the CPU):
  base 3.41M, 0001 and 0001+0002 20.89M, final 0.60M alone.
- On this CPU the cold regression of 0001 / 0001+0002 alone is smaller (1.35x / 1.21x slower than base, vs
  1.47x in the single runs above / 1.60x in the paired cold A/B on the study host), and the final series is 1.50x faster than base cold.
- With the tunables, or after one other grammar, all patched builds are faster than base: the faults, not the
  patches' compute, are what made the cold case slow.
