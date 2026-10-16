# Who builds the full grammar mask, and how often

Audit of every in-tree caller of the grammar sampler at the pinned SHA `620d2225`. All paths are
relative to `third_party/llama.cpp`. The optimizations in this study speed up
`llama_grammar_apply_impl` on a full-vocab candidate array; how much a user sees depends on how
often their code path builds that array.

## Every step (the paths that benefit most)

| path | file:line | why it is every step |
|---|---|---|
| `llama-speculative` example, draft model | `examples/speculative/speculative.cpp:530` (`common_sampler_sample(..., true)`), sampler built from the user's `params.sampling` at `:235` | `grammar_first = true` applies the grammar to the full candidate array before the chain (`common/sampling.cpp:646-648`), on every draft token, and the draft sampler carries the user's `--grammar` |
| `llama-speculative` example, target verification (temp > 0, one draft sequence) | `examples/speculative/speculative.cpp:277`, sampler built at `:228` | same, on the target model, every verified position |
| any application that puts the grammar sampler in a `llama_sampler` chain (public API) | chain apply `src/llama-sampler.cpp:681-700` calls every sampler on the full `cur_p`; grammar sampler `src/llama-sampler.cpp:2686-2690` calls `llama_grammar_apply_impl`; constructors `include/llama.h:1530`, `:1549` | the chain has no "check the sampled token first" step; whatever array the caller passes (usually the whole vocab) is masked every step |
| `common_sampler_sample(..., grammar_first = true)` in general | `common/sampling.cpp:606`, `:646-648`; declared `common/sampling.h:65-68` | documented as "the grammar is applied before the samplers (slower)" |

## Only when the sampled token is rejected (the default path)

| path | file:line | cost |
|---|---|---|
| `common_sampler_sample(..., grammar_first = false)` | single-token check `common/sampling.cpp:658-670`, full mask + resample on reject `:672-682` | full mask on ~5% of steps in our end-to-end runs (results/e2e/base/qwen05_summary.md), 0-64% per grammar |
| `llama-server` (normal decoding) | `tools/server/server-context.cpp:4468` (`common_sampler_sample(slot.smpl.get(), ...)`, default `grammar_first`) | default path |
| `llama-server` speculative verification | `tools/server/server-context.cpp:4541` (`common_sampler_sample_and_accept_n_rejection`), `:4543` (`common_sampler_sample_and_accept_n`) -> `common/sampling.cpp:700`, `:714`, `:753`, `:832` with default `grammar_first = false` | default path per verified token |
| `llama-completion` | `tools/completion/completion.cpp:667` | default path |

## Paths that look like "every step" but do not pay the full mask

| path | file:line | why not |
|---|---|---|
| speculative drafters in `common/speculative.cpp` (draft-simple, EAGLE-3, DFlash, MTP) | `grammar_first = true` at `common/speculative.cpp:406`, `:854`, `:1379`, `:1398`, `:1773` | their samplers are built from a fresh `common_params_sampling` with **no grammar** (`:61-69`, `:275-280`, `:575-580`, `:1112`, `:1500`), so `grammar_should_apply` (`common/sampling.cpp:461-464`) is false and no mask is built. **Correction:** results/profile.md (Phase 2) said speculative-draft sampling pays the full mask every step; that is only true for the `llama-speculative` example, not for the server's drafters. |
| rejection-sampling verification mask | `common/sampling.cpp:758-763` | applies the grammar to the post-chain candidate list (`common_sampler_get_candidates(gsmpl, true)` at `:755`), which is already cut to top-k, not to the full vocab |
| backend sampling | `common/sampling.cpp:422-426` | disabled when a grammar is present |
| llguidance backend | `common/sampling.cpp:217-219`, `common/llguidance.cpp` | different engine; not affected by these patches |

## What this means for the headline numbers

The headline speedups in this study are for **mask computation** (one full-vocab
`llama_grammar_apply_impl` call), measured by vocab-only replay. They translate into end-to-end
throughput roughly one-for-one only on the "every step" paths above. On the default path used by
`llama-server` and `llama-completion`, grammar is ~6% of generation time with this model, so the
end-to-end throughput gain there is about 2% (measured in Phase 4). The default path still benefits
on rejected steps, which are the latency spikes: a rejection at a high-stack position costs a full mask
(0.2-1.6 s per step at baseline on the worst grammars).
