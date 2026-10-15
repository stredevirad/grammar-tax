# Workbench rules (survive context compaction)

This repo is a private study of llama.cpp GBNF grammar sampling performance on CPU.

1. NEVER commit, push, open PRs/issues/comments on ggml-org/llama.cpp or any upstream repo. Never write upstream commit messages or PR descriptions. All pushes go only to this workbench repo (branch `claude/nice-bardeen-at3kfm`).
2. llama.cpp is a submodule at `third_party/llama.cpp`, pinned (SHA in README.md). Never commit changes inside it. Every change is a `patches/NNNN-short-name.patch` (git format-patch style), applied by `scripts/apply_patches.sh` onto the pinned SHA (into a separate worktree, `build/src-patched`).
3. Patch C++ follows llama.cpp CONTRIBUTING.md / AGENTS.md / skills/code-review/SKILL.md: simple C++, no fancy templates, 4-space indent, snake_case, ASCII only, short comments, no new deps, smallest change for most of the win.
4. Frugal: Release builds, only needed targets, ccache, -j4. No full rebuilds for small changes. No subagents unless truly parallel. Write big logs to files, summarize.
5. Commit + push at end of each phase (and mid-phase). Keep PHASE_LOG.md current. On "continue": read PHASE_LOG.md and DECISIONS.md first.
6. Honesty: report medians + spread + run count. No cherry-picking. Report non-wins.
7. Correctness bar: masks must be bit-identical (`grammar_replay --check`) on every step of every trace. Any difference is a bug.
8. User said: STOP after Phase 2 GATE, write results/profile.md with recommendation, push, wait for go-ahead before Phase 3.
