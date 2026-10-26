# UPSTREAM.md - checklist if you decide to submit to llama.cpp yourself

Everything here is for you to do by hand. Nothing in this repo has been, or will be, sent
upstream by an agent. llama.cpp forbids automated submissions and AI-written PR text,
commit messages and reviewer replies (AGENTS.md, CONTRIBUTING.md "AI Usage Policy"); breaking
that can get you banned. This file deliberately contains no PR description and no commit
message.

## 0. Decide whether it is worth a PR at all

- [ ] Re-read `results/REPORT.md`. Is the win real on the path users hit? Remember PR #6811 was
      closed because the end-to-end gain was not measurable once the resample fast path
      (#7424) existed. Have numbers for the full-mask path *and* for end-to-end generation, and
      say which users hit the full mask every step (`results/who_benefits.md`: the
      `llama-speculative` example, `grammar_first = true`, the grammar sampler inside a
      `llama_sampler` chain; **not** llama-server's speculative drafters, which carry no grammar).
      On the default path the throughput gain is about 2%; say so in the PR.
- [ ] Can you explain every line of the patch without notes? CONTRIBUTING.md says to budget
      about one hour of review per 200-400 lines, and you must be able to answer maintainers
      without AI help. Work through `LEARNING.md` Part 2 until you can.
- [ ] Is it one idea? The series is three patches. **0001 must not go upstream without 0003**: 0001 alone makes
      stack-heavy grammars up to ~60% slower in a fresh process (glibc page faults, `results/REPORT.md` section 4),
      and 0003 fixes that. Either submit 0003 first (it is a small improvement on its own: it also cuts the
      baseline's page faults) and then 0001, or submit them together as one PR and explain why. 0002 is
      independent and small (+3.5%); decide whether it is worth a reviewer's time on its own.
- [ ] Measure the cold case yourself: replay a stack-heavy schema alone in a fresh process
      (`scripts/cold_ab.sh`), and run one end-to-end generation per process. Reviewers on other platforms
      (macOS, Windows, musl, other allocators) may see different allocator behavior; say what you tested
      (section 4a: this study tested glibc only).

## 1. Check for duplicates (again, right before submitting)

- [ ] Search open and closed PRs: `grammar`, `llama-grammar`, `decode_utf8`, `reject_candidates`,
      `advance_stack`, `match_char`, `codepoints`. See the table in `DECISIONS.md` ("Upstream
      overlap") for what existed on 2026-10-15, then look for anything newer.
- [ ] Check open PR #28371 (memoize reject_candidates_for_stack) and anything else touching
      `src/llama-grammar.cpp`. If someone is on the same idea, comment on their PR to work
      together instead of opening a duplicate (CONTRIBUTING.md, AI policy point 2).
- [ ] Look at issue #4218 (grammar speed umbrella issue) for recent maintainer direction.
- [ ] **#28371 (open, memoization) overlaps with 0002 and has a correctness problem at its head.** At
      commit f000de6 its cache keys keep raw pointers into per-call decode buffers across steps; on our
      workload that gives wrong masks and crashes (valgrind memcheck: invalid reads and uninitialised values in
      the cache's hashing and lookup, `llama_grammar_candidates_hash`, DECISIONS.md D3.11). Its earlier commit c29085b (cache cleared every
      call) is correct and is the version measured in results/REPORT.md. Whether and how to tell the
      author is your decision; if you do, write the comment yourself, keep it short and factual, and offer
      the reproduction (`scripts/check_masks.sh`, `valgrind --tool=memcheck` on `grammar_replay`). Do not
      paste text from this repo as if it were your own words.

## 2. Open an issue first if needed

- [ ] Performance changes to existing code are not "features", but if the change adds a field
      to `llama_grammar`, changes memory use, or touches the public API, open an issue first,
      describe the measurement and the idea in your own words, and wait for a maintainer signal.
- [ ] As a new contributor: at most 1 open PR at a time, no trivial PRs.

## 3. Prepare the branch yourself

- [ ] Fork ggml-org/llama.cpp on your own account (not this workbench repo).
- [ ] `git fetch upstream && git switch -c <your-branch> upstream/master`.
- [ ] Apply the patch by hand (`git am patches/NNNN-*.patch` or retype it). Resolve conflicts
      yourself; master moves fast, the pinned SHA here is from 2026-10-15.
- [ ] Read the diff line by line against the surrounding code style (4 spaces, snake_case,
      ASCII only, short comments, no new deps, `void * ptr` / `int & a`).
- [ ] Run `skills/code-review/SKILL.md` from the upstream repo as a self-review checklist,
      especially the security section (reserved buffers and stored pointers into buffers).

## 4. Re-run the evidence yourself, on current master

- [ ] Build Release, run `test-grammar-parser`, `test-grammar-integration`, `test-llama-grammar`,
      `test-json-schema-to-grammar` (and `test-grammar-llguidance` if you build llguidance).
- [ ] If the change fixes a bug, add a regression test that fails before and passes after.
- [ ] Re-run this repo's replay benchmark A/B against *current* master (point `LLAMA_SRC` at your
      checkout) and the `--check` bit-exact mask comparison. Numbers in this repo are from one
      4-vCPU VM; yours will differ. Report median, spread and run count.
- [ ] Run `llama-bench` without a grammar to show no regression, and an end-to-end generation
      with a JSON schema (`--json-schema`) to show the user-visible effect.
- [ ] Do the CONTRIBUTING.md steps in section 4a, which this study did not do.

## 4a. CONTRIBUTING.md steps NOT done in this study

Everything in this repo except the latest quick-verify run was measured on one 4-vCPU Linux x86-64 VM, GCC 13.3,
**glibc 2.39 malloc**. The study ran on a "Xeon @ 2.80GHz" host; the cold-fault re-run, the baseline memcheck
re-run and the first `results/quick-verify/` run ran on a second host after a VM restart ("Xeon @ 2.10GHz", also
AVX-512; REPORT section 7). The latest quick-verify run (2026-10-26) used a laptop: Core Ultra 5 125H (AVX2, no
AVX-512) under WSL2 with Debian 12's GCC 12.2 and glibc 2.36. These CONTRIBUTING.md steps were not run and are
yours to do before publishing:

- [ ] **Run the full CI locally** (CONTRIBUTING.md: "Execute the full CI locally on your machine before
      publishing", see `ci/README.md`): CPU-only is `mkdir tmp && bash ./ci/run.sh ./tmp/results ./tmp/mnt`
      from the upstream checkout. It builds Debug and Release, runs the whole `ctest` suite in both, and
      downloads a small model (Qwen3-0.6B) plus wikitext for quantize / generation / perplexity checks; this
      repo ran only the four grammar tests (`scripts/run_tests.sh`). Not done here.
- [ ] **Test on a non-glibc platform: macOS (libmalloc) or Windows (MSVC / UCRT heap).** Not done here.
      This matters for this series specifically, because part of the result depends on the allocator:
      - 0003's main benefit is a fix for a **glibc-specific** cold-start effect (glibc's dynamic mmap
        threshold turns each ~4.8 MB per-stack `reserve` into mmap + page faults + munmap; DECISIONS.md
        D3.13, D3.15). On another allocator, 0001 alone may not regress cold, and 0003 may give little.
        Measure both: warm (`scripts/run_bench.sh`) and cold (`scripts/cold_ab.sh` on a stack-heavy
        schema such as o46322, and one end-to-end generation per fresh process).
      - 0001's warm gain comes from removing ~152k small `malloc`/`free` pairs per mask; how much that
        saves depends on the allocator's small-block cost, so 0001's ~1.46x (and the series' 1.47x) may be
        larger or smaller there.
      - WSL2 does **not** count: it runs a Linux kernel with glibc, the same allocator as this study.
        musl (e.g. Alpine) is a third allocator worth a run if you have it.
      - The scripts here assume bash and GNU coreutils (`split -n l/N`, `date -Is`); on macOS install
        coreutils or port the few commands; on Windows build `grammar_replay` with MSVC and drive it by hand.
- [ ] **`llama-perplexity`** (CONTRIBUTING.md asks for perplexity and `llama-bench`): only `llama-bench`
      was run here (no change, `results/REPORT.md`). Perplexity does not use the grammar code, so no
      change is expected, but say whether you ran it.
- [ ] **Other compilers and CPUs**: the study used GCC 13.3 on two AVX-512 Xeon hosts; only
      `scripts/quick_verify.sh` also ran with GCC 12.2 on an AVX2 laptop CPU (masks identical, 1.590x on its
      20-grammar subset). Its ratios came out 7-9% above `ab-final` on all three CPUs (REPORT section 7).
      Clang and MSVC builds, and an ARM machine (e.g. Apple Silicon, which also covers the macOS item), are
      not covered.
- `test-backend-ops` (CONTRIBUTING.md, for `ggml` changes) does not apply: the series touches only
  `src/llama-grammar.cpp`.

## 5. Write the PR yourself

- [ ] Follow `.github/pull_request_template.md` in the upstream repo.
- [ ] Write the description and commit message in your own words. Short and direct; maintainers
      read many PRs. Include: what was slow, how you measured, the numbers, how correctness was
      checked.
- [ ] **Disclose AI assistance** in the template's disclosure section: say plainly that an AI
      agent built the benchmark harness and drafted the patch, and that you reviewed, re-ran and
      understand it. Undisclosed AI use can lead to a permanent ban.
- [ ] Commit title format used by maintainers when squashing: `grammar : <short title>`.
- [ ] If an AI writes any part of a commit on your behalf, upstream asks for
      `Assisted-by: <assistant name>` (not `Co-authored-by`). Simplest: write it yourself.

## 6. After submitting

- [ ] Answer reviewers yourself, without AI-written replies.
- [ ] Rebase on master if it goes stale; re-run the numbers if the surrounding code changed.
- [ ] Only update `RESUME.md` wording from "benchmarked patch" to "merged upstream" once it is
      actually merged, and link the merged PR.
