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
      say which users hit the full mask every step (speculative draft sampling,
      `grammar_first`).
- [ ] Can you explain every line of the patch without notes? CONTRIBUTING.md says to budget
      about one hour of review per 200-400 lines, and you must be able to answer maintainers
      without AI help. Work through `LEARNING.md` Part 2 until you can.
- [ ] Is it one idea? Each patch in `patches/` is meant to be one PR. Do not bundle.

## 1. Check for duplicates (again, right before submitting)

- [ ] Search open and closed PRs: `grammar`, `llama-grammar`, `decode_utf8`, `reject_candidates`,
      `advance_stack`, `match_char`, `codepoints`. See the table in `DECISIONS.md` ("Upstream
      overlap") for what existed on 2026-10-15, then look for anything newer.
- [ ] Check open PR #28371 (memoize reject_candidates_for_stack) and anything else touching
      `src/llama-grammar.cpp`. If someone is on the same idea, comment on their PR to work
      together instead of opening a duplicate (CONTRIBUTING.md, AI policy point 2).
- [ ] Look at issue #4218 (grammar speed umbrella issue) for recent maintainer direction.

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
- [ ] CONTRIBUTING.md asks to run the full CI locally (`ci/README.md`) before publishing.

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
