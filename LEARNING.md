# LEARNING.md - how llama.cpp's grammar engine works

Written for a reader who is strong in C++ but new to this code. Every claim points at
`third_party/llama.cpp/src/llama-grammar.cpp` (pinned SHA in README.md). Line numbers are
for that SHA.

> Status: Part 1 (the engine) is written. Part 2 (patch walkthrough) and Part 3 (interview
> questions) are written after Phase 3, from the patches that were actually kept.

## Part 1. The engine

### 1.1 What problem it solves

At each generation step the model outputs one logit per vocab token (151,936 tokens for
Qwen2.5). A grammar sampler must set the logit of every token that would break the grammar to
`-inf`, so whatever sampler runs next can only pick legal tokens. After a token is chosen, the
grammar must "consume" it, so the next step knows where we are in the grammar.

So there are exactly two operations:

- **apply** (`llama_grammar_apply_impl`, line 1359): given the current state, mark every
  illegal token. Cost grows with vocab size.
- **accept** (`llama_grammar_accept_impl`, line 1402): advance the state by one chosen token.
  Cost grows with the token length and the number of live parse states.

Our benchmark times both, but apply is >99% of the cost (see results/baseline.md).

### 1.2 Rules are flat arrays of elements

A GBNF grammar such as

```
root ::= "[" item ("," item)* "]"
item ::= [a-z]+
```

is parsed (`llama_grammar_parser`, line 420 onward) into **rules**. Each rule is a
`std::vector<llama_grammar_element>`, and an element is just `{type, value}`
(`llama-grammar.h` line 47):

| type | value | meaning |
|---|---|---|
| `CHAR` | code point | match this char; may be followed by `CHAR_ALT` / `CHAR_RNG_UPPER` to form a class like `[a-zA-Z_]` |
| `CHAR_NOT` | code point | negated class `[^"\\]` |
| `CHAR_ANY` | - | `.` |
| `RULE_REF` | rule id | non-terminal: "now match rule N" |
| `ALT` | - | separates alternatives of one rule (`a | b`) |
| `END` | - | end of the rule |
| `TOKEN` / `TOKEN_NOT` | token id | match a specific token id (`<[123]>`) |

Repetition (`*`, `+`, `{m,n}`) is rewritten by the parser into extra helper rules, so at
runtime there are only sequences, alternatives, references and terminals. A char class is a
run of elements: `[a-z0-9]` is `CHAR 'a', CHAR_RNG_UPPER 'z', CHAR_ALT '0', CHAR_RNG_UPPER '9'`.

### 1.3 The parse state is a set of stacks

The engine is a **non-deterministic pushdown recognizer**. One parse position is a **stack**:
`std::vector<const llama_grammar_element *>`. Each entry points *into* a rule array:

- `stack.back()` is the next terminal we expect (a char class or token element).
- the entries below it are "return addresses": where to continue in the calling rules once
  the current rule finishes.

Because a grammar can be ambiguous (several alternatives might still be possible), the state is
a **list of stacks**, `llama_grammar::stacks`. An empty stack means "the grammar can end here",
which is how end-of-generation (EOG) is allowed (line 1366).

Pointers into `rules` are why `llama_grammar_clone_impl` (line 1329) has to remap every pointer
after copying the rules, and why the stacks can be compared by pointer value.

### 1.4 advance_stack: expand until a terminal is on top

`llama_grammar_advance_stack` (line 860) takes one stack whose top may be a `RULE_REF` and
expands it into all stacks whose top is a terminal:

- top is a `RULE_REF` to rule R: pop it; for each alternative of R, push "the element after the
  reference" (the return address, unless it is the end of the sequence) and then the first
  element of that alternative. Push each result on a `todo` list.
- top is a terminal: keep this stack (output).
- stack is empty: keep it (the grammar may end).

Since PR #18604 (2026-03) this is iterative (an explicit `todo` vector instead of recursion) and
it de-duplicates with a `std::set<stack>` ordered by comparing the pointer sequences, so cyclic
nullable rules cannot loop forever. The output list is also de-duplicated with a linear
`std::find`. The author measured this as ~13% slower than the old code on
test-grammar-integration; in our workload `advance_stack` is a small share (see profile.md).

### 1.5 accept: consume one token

`llama_grammar_accept_token` (line 1476) decodes the token's text to Unicode code points
(`decode_utf8`) and feeds them one at a time through every stack:

- `llama_grammar_accept_chr` (line 1024): if the stack's top terminal matches the char
  (`llama_grammar_match_char`, line 765), pop it, push the next element of the same sequence if
  there is one, then `advance_stack` again so the top is a terminal.
- stacks whose top does not match are dropped.
- surviving stacks are de-duplicated with `std::find` into the new stack list.

UTF-8 detail: a token may end in the middle of a multi-byte character. The leftover bits are kept
in `grammar.partial_utf8` and the next token's decode continues from them.

### 1.6 apply: the full-vocab mask (where the time goes)

`llama_grammar_apply_impl` (line 1359), per step:

1. For **every** vocab token: look up its text (`token_to_piece`, a cached string), check
   `is_eog` (a `std::set` lookup), and **decode the text to code points into a fresh
   `std::vector<uint32_t>`** (`decode_utf8`, line 34). With 151,936 tokens that is ~152k heap
   allocations and ~152k frees per step.
2. Build `candidates`: `{index, pointer to code points, partial_utf8, id}`.
3. `llama_grammar_reject_candidates` (line 944): run the candidate list through each stack in
   turn; a token survives if *any* stack accepts it, so the rejects of stack 1 become the input
   of stack 2, and so on.
4. Set the logit of every rejected token to `-inf`.

The core is the recursive `llama_grammar_reject_candidates_for_stack` (line 1061). For one stack
and a list of candidates that have already matched `k` code points:

- candidates with no code points left are accepted (unless they end in an impossible partial
  UTF-8 sequence);
- otherwise test the next code point against the stack's top terminal (`match_char`). Fails are
  rejected; passes go to `next_candidates` with the code point pointer advanced by one;
- compute the stack after that terminal (`advance_stack`), and recurse on the passes with each
  resulting stack.

So the work is roughly *(number of stacks) x (number of tokens still alive at each char
depth)*, plus the per-step decode of the whole vocab. For a string-content position in JSON
(`[^"\\]`) almost every token matches every char, so the recursion walks the full text of
almost every vocab token.

### 1.7 How it is used in practice (why the full mask is not run every step)

`common_sampler_sample` (common/sampling.cpp, line 606) does **not** normally run apply on the
whole vocab. It first samples a token with the normal chain, then runs apply on a one-element
array containing just that token. Only if that token is illegal does it rebuild the full
candidate list, apply the grammar to all of it, and sample again. So:

- per step, cost = cheap single-token check, plus (sometimes) one full mask;
- with `grammar_first = true`, or when the grammar sampler sits inside a `llama_sampler` chain, the full
  mask runs every step. In-tree, that is the `llama-speculative` example; the speculative drafters used by
  `llama-server` carry no grammar (full audit with file:line: `results/who_benefits.md`).

The replay benchmark measures the full mask on every step, i.e. the slow path. The end-to-end
runs measure how often that slow path happens for real.

### 1.8 Reference points

- **llguidance** (optional backend, `-DLLAMA_LLGUIDANCE=ON`): a Rust library that compiles the
  grammar to a lexer plus an Earley parser and walks a token trie, so shared token prefixes are
  checked once. On our traces it takes ~0.3-0.4 ms per token versus tens of ms for the built-in
  engine.
- **XGrammar**: splits tokens into context-independent ones (decided at compile time) and
  context-dependent ones (checked at runtime), so most of the mask is precomputed.

Both get their speed from structure (tries, precomputation, a different parser), not from
micro-optimizing the loop. Porting either is out of scope; small ideas are borrowed only where
they fit llama.cpp's simplicity rules.

## Part 2. Patch walkthrough

The kept patches live in `patches/` and apply in order to the pinned SHA
(`scripts/apply_patches.sh`): **0001** (one decode buffer), **0002** (ASCII match table), **0003** (reuse
the reject lists across stacks). Rejected experiments are in `patches/rejected/` and explained after the
kept ones. Numbers quoted here come from `results/REPORT.md`; this part is about
*why the code is correct and why it is faster*.

### Patch 0001: decode all candidate pieces into one buffer

**Problem.** Every call to `llama_grammar_apply_impl` built

```cpp
std::vector<std::pair<std::vector<uint32_t>, llama_partial_utf8>> candidates_decoded;
...
candidates_decoded.push_back(decode_utf8(piece, grammar.partial_utf8));
```

so each of the ~152k vocab tokens got its own heap-allocated `std::vector<uint32_t>` holding its code
points, and all of them were freed when the function returned. That is ~152k `malloc` + ~152k `free` per
step (11.6 MB of heap in 151,659 allocations for the Qwen vocab), and it was ~35% of all instructions on the
median grammars (callgrind) and ~26% of the time (perf), mostly inside glibc (`_int_malloc`, `_int_free`,
`malloc_consolidate`).

**Change 1: an appending decoder.**

```cpp
// appends the code points of src and a terminating 0 to code_points
// adds at most src.size() + 1 elements
static llama_partial_utf8 decode_utf8(
        const std::string & src,
        llama_partial_utf8 partial_start,
        std::vector<uint32_t> & code_points) {
    ...
    const size_t          start    = code_points.size();
```

- The body is the old decoder line for line; only the output changes. Instead of building and returning
  a fresh vector, it appends to the caller's vector and returns just the partial-UTF-8 state.
- `start` remembers where this token's code points begin, because the old code had one abort path that
  did `code_points.clear(); code_points.push_back(0);` ("invalid sequence: this token decodes to nothing").
  With a shared buffer, `clear()` would wipe every earlier token, so it becomes
  `code_points.resize(start); code_points.push_back(0);`: drop only this token's code points. The other
  abort path (an invalid continuation byte) already pushed nothing for this token before aborting, so it
  stays `push_back(0)`.
- **Why "at most src.size() + 1"**: every code point the loop emits consumes at least one byte of `src`
  (a continued partial sequence also consumes >= 1 byte before it completes), and the only other element
  is the terminating 0. Both abort paths emit exactly one element. This bound is what makes the reserve in
  change 3 safe.

**Change 2: the old signature becomes a wrapper.**

```cpp
static std::pair<std::vector<uint32_t>, llama_partial_utf8> decode_utf8(
        const std::string & src,
        llama_partial_utf8 partial_start) {
    std::vector<uint32_t> code_points;
    code_points.reserve(src.size() + 1);
    const llama_partial_utf8 partial = decode_utf8(src, partial_start, code_points);
    return std::make_pair(std::move(code_points), partial);
}
```

`llama_grammar_accept_token` and `llama_grammar_accept_str` decode one piece per step and keep using this
signature, so accept is untouched. With an empty vector, `resize(start)` is `resize(0)`, i.e. the old
`clear()`, so the wrapper behaves exactly like the old function.

**Change 3: one buffer per apply call.**

```cpp
size_t n_code_points = 0;
for (size_t i = 0; i < cur_p->size; ++i) {
    n_code_points += grammar.vocab->token_to_piece(cur_p->data[i].id).size() + 1;
}
std::vector<uint32_t> code_points;
code_points.reserve(n_code_points);
const uint32_t * code_points_data = code_points.data();
...
        const size_t offset = code_points.size();
        const llama_partial_utf8 partial_utf8 = decode_utf8(piece, grammar.partial_utf8, code_points);
        candidates_grammar.push_back({ i, code_points.data() + offset, partial_utf8, id });
...
GGML_ASSERT(code_points.data() == code_points_data);
```

- The first loop sums the per-token bound over *all* candidates (including EOG and empty pieces, which are
  never decoded). Over-counting a few hundred tokens is cheaper and simpler than repeating the
  EOG/empty checks. For Qwen this is 4.5 MB.
- `llama_grammar_candidate::code_points` is a raw pointer into the buffer. A `std::vector` that grows
  reallocates and invalidates every pointer into it; because of the reserve and the per-token bound it
  never grows here. The assert checks that the data pointer did not move before the candidates are used,
  so a wrong bound would abort instead of becoming a use-after-free (the upstream code-review checklist
  calls out exactly this "reserve then assume" pattern).
- `offset` is taken before the decode, so `code_points.data() + offset` points at this token's first code
  point, and the token's code points end at its terminating 0: the same layout each per-token vector had.

**Why it is correct.** For every candidate, the values reachable through `code_points` (up to and
including the 0) and the returned `partial_utf8` are exactly what the old per-token vector held: same
decoder, same input, same output values, now stored at a different address. `index` and `id` are
unchanged. `llama_grammar_reject_candidates` only reads through the pointer, so it sees identical inputs
and produces the identical reject set. The bit-exact check (every step of all 206 traces, both vocabs) and
the random-walk differential test confirm it.

**Why it is faster.** ~152k `malloc`/`free` pairs per step become one allocation. The code points are also
contiguous and in vocab order, so every later pass that reads `*cand.code_points` (once per candidate per
stack) streams through memory instead of chasing 152k scattered heap blocks.

**How it differs from upstream PR #6811 (closed 2024).** #6811 cached the decoded code points of every
token *across steps* (on the vocab/context, later lazily with a mutex). That raised memory (~2x vocab) and
concurrency concerns and, measured end to end after the resampling fix #7424, showed no gain. Patch 0001
keeps nothing between calls, needs no locking, and lowers the transient memory of a call (4.5 MB in one
block instead of 11.6 MB in 152k blocks). It still decodes every step; the decode compute itself (~14% of
time on the median set) is untouched.

### Patch 0002: precompute the char match for ASCII in long candidate lists

**Problem.** After 0001, `llama_grammar_match_char` is the next biggest piece (about 18% of time on the
worst grammars). For every candidate, for every stack, it walks the char class at the top of the stack
(`[^"\\\x7F\x00-\x1F]` is four elements) to decide whether the candidate's first code point matches.

**Change** (inside `llama_grammar_reject_candidates_for_stack`, before the per-candidate loop):

```cpp
// the match result depends only on the code point, so for long lists precompute it for ascii
bool match_ascii[128];
const bool use_ascii = candidates.size() >= 128;
if (use_ascii) {
    for (uint32_t chr = 0; chr < 128; ++chr) {
        match_ascii[chr] = llama_grammar_match_char(stack_pos, chr).first;
    }
}

for (const auto & tok : candidates) {
    const uint32_t chr = *tok.code_points;
    if (chr == 0) {
        ...
    } else if ((use_ascii && chr < 128) ? match_ascii[chr] : llama_grammar_match_char(stack_pos, chr).first) {
```

- `llama_grammar_match_char(pos, chr)` is a pure function of the stack element and the code point, so a
  table filled by calling it is exactly equal to calling it again: same results, same order of output.
- Building the table costs 128 calls, so it is only built when the list has at least 128 candidates (the
  top-level lists have ~152k; the deep recursion lists are usually small).
- Code points >= 128 (multibyte characters) still go through the function.

**Measured.** +3.6% and +3.3% per grammar over 0001 in two separate A/B sessions (total time -3.1% and
-4.2%). Small, but consistently above the 3% bar, for 13 lines.

### Patch 0003: reuse the reject lists across stacks

**Problem (found by measuring, not by reading).** Replaying the 600-stack schema *alone in a fresh process*,
0001+0002 was ~60% slower than the baseline, with 20.9 million page faults (baseline 3.4 million). The cause
is pre-existing code:

```cpp
auto rejects = llama_grammar_reject_candidates_for_stack(rules, stacks.front(), candidates);
for (size_t i = 1, size = stacks.size(); i < size; ++i) {
    rejects = llama_grammar_reject_candidates_for_stack(rules, stacks[i], rejects);
}
```

Each call creates a new `rejects` vector reserved to `candidates.size()` (~152k x 32 bytes = 4.8 MB at the top
level) and fills it. glibc serves large allocations with `mmap` until its *dynamic* mmap threshold has been
raised by earlier large frees, and returns memory at the top of the heap to the kernel above a trim
threshold. In a fresh process every per-stack list is therefore new memory: ~1,200 page faults (the kernel
zeroes each 4 KB page) per stack per step. In the baseline, ~152k small per-token vectors are live during the
sweep and change what glibc does with those blocks; 0001 removed them and exposed the problem. With
`GLIBC_TUNABLES` raising both thresholds the faults disappear and 0001+0002 is faster than the baseline
again, which confirms the mechanism.

**Change.**

```cpp
llama_grammar_candidates llama_grammar_reject_candidates_for_stack(rules, stack, candidates) {
    llama_grammar_candidates rejects;
    llama_grammar_reject_candidates_for_stack_into(rules, stack, candidates, rejects);
    return rejects;
}

// writes the rejected candidates into rejects (cleared first), so the caller can reuse its storage
static void llama_grammar_reject_candidates_for_stack_into(rules, stack, candidates, rejects) {
    rejects.clear();
    rejects.reserve(candidates.size());
    ... the old body, with `return rejects;` -> `return;`
}

// in llama_grammar_reject_candidates:
llama_grammar_candidates rejects;
llama_grammar_candidates rejects_next;
llama_grammar_reject_candidates_for_stack_into(rules, stacks.front(), candidates, rejects);
for (size_t i = 1, size = stacks.size(); i < size; ++i) {
    llama_grammar_reject_candidates_for_stack_into(rules, stacks[i], rejects, rejects_next);
    std::swap(rejects, rejects_next);
}
return rejects;
```

- The body moves into a `static` function that writes into a vector the caller owns. `clear()` keeps the
  capacity, so `reserve()` is a no-op once the vector has been big enough once.
- The per-stack loop alternates two vectors (input of stack *i* is the output of stack *i-1*; they must not
  be the same vector, hence two). `std::swap` of two vectors swaps three pointers, no copying.
- Result: two big allocations per apply call instead of two per stack.
- The public `llama_grammar_reject_candidates_for_stack` (declared in `llama-grammar.h` and called by
  `test-llama-grammar`) becomes a 3-line wrapper with the same output in the same order.

**Correctness.** The body is unchanged except where the output lives; the caller reads the same elements in
the same order. Bit-identical masks on all traces and in the random-walk test.

**Measured.** Cold 600-stack schema: page faults 20.9M -> 0.6M, 377.6 -> 180.3 ms per token (baseline 235 ms).
Full numbers in `results/REPORT.md`.

## Rejected experiments

### Rejected: P2, skip candidates whose first char cannot match the stack

> **Status: rejected.** Correct (bit-identical masks) but 2.4% *slower* than P1 alone; see "Why it did not
> work" at the end of this section and `results/experiments.md`. It is explained in full because the
> correctness argument is the core of every "pre-filter" idea and is a likely interview topic.

**Problem.** The old top level was

```cpp
auto rejects = llama_grammar_reject_candidates_for_stack(rules, stacks.front(), candidates);
for (size_t i = 1; i < stacks.size(); ++i) {
    rejects = llama_grammar_reject_candidates_for_stack(rules, stacks[i], rejects);
}
```

"Rejected by stack 1" feeds stack 2, and so on. At a typical step ~99.9% of tokens fail their first
character for every stack, so every stack receives ~152k candidates, tests each one, and copies almost all
of them into a new `rejects` vector (32 bytes each, ~4.8 MB per stack). With hundreds of live stacks (a JSON
object with 200 optional properties) that is gigabytes of copying per step; it was 63% of the time on the
worst grammars.

**Change.**

```cpp
std::vector<uint8_t> accepted(cur_p->size, 0);
llama_grammar_candidates candidates_stack;

for (const auto & stack : grammar.stacks) {
    const llama_grammar_element * pos = stack.empty() ? nullptr : stack.back();
    const bool is_char = pos != nullptr && (pos->type == LLAMA_GRETYPE_CHAR || pos->type == LLAMA_GRETYPE_CHAR_NOT || pos->type == LLAMA_GRETYPE_CHAR_ANY);

    candidates_stack.clear();
    for (const auto & cand : candidates_grammar) {
        if (accepted[cand.index]) {
            continue;
        }
        if (is_char && *cand.code_points != 0 && !llama_grammar_match_char(pos, *cand.code_points).first) {
            continue;
        }
        candidates_stack.push_back(cand);
    }
    if (candidates_stack.empty()) {
        continue;
    }

    const auto rejects = llama_grammar_reject_candidates_for_stack(grammar.rules, stack, candidates_stack);
    for (const auto & cand : candidates_stack) {
        accepted[cand.index] = 1;
    }
    for (const auto & reject : rejects) {
        accepted[reject.index] = 0;
    }
}

for (const auto & cand : candidates_grammar) {
    if (!accepted[cand.index]) {
        cur_p->data[cand.index].logit = -INFINITY;
    }
}
```

- `accepted` is indexed by `cand.index`, the candidate's position in `cur_p`, so it can be written and read
  in O(1).
- For each stack, `candidates_stack` gets only the candidates that are not yet accepted and that pass the
  pre-check. Everything else is skipped for this stack.
- The unchanged `llama_grammar_reject_candidates_for_stack` runs on that subset. The subset's members were
  not accepted before, so "accepted = subset minus rejects" is written as "set all of the subset, then clear
  the rejects".
- At the end, a candidate is masked iff no stack accepted it.

**The pre-check, and why it may only skip "definitely rejected".**
`llama_grammar_reject_candidates_for_stack(stack, list)` handles one candidate `c` like this (line
references at the pinned SHA):

1. stack empty (`src/llama-grammar.cpp:1069-1076`): reject iff `c` has code points or a pending partial
   sequence;
2. top is `TOKEN`/`TOKEN_NOT` (`:1080-1094`): compare the token id;
3. top is a char element (`:1099-1112`) (`CHAR`, `CHAR_NOT`, `CHAR_ANY`; ranges and alternates are part of the same element
   run): if `c` has no complete code point left (`*c.code_points == 0`, e.g. a byte-fallback token or a
   token that only continues a partial UTF-8 sequence), decide with `llama_grammar_match_partial_char`;
   otherwise **reject immediately if `llama_grammar_match_char(top, first code point)` fails**, and only
   if it matches, advance the stack and recurse on the rest of the token.

The pre-check skips `c` only in the bolded case and calls the *same* `llama_grammar_match_char` on the
*same* inputs, so a skipped candidate is one the full check would reject at its first step. Every other case
(empty stack, token tops, no complete code point, invalid UTF-8, which decodes to `{0}` with
`n_remain = -1`) is not pre-checked at all; those candidates always go to the full check. Char ranges,
negated classes `[^...]`, alternates and `.` are all handled inside `llama_grammar_match_char`, so the
pre-check treats them exactly as the full check does. Empty-piece and EOG tokens never become candidates
(handled earlier in `apply_impl`), so they are unaffected.

**Why the whole mask is identical.** For a fixed stack `s`, whether `c` ends up in
`llama_grammar_reject_candidates_for_stack(s, L)` depends only on `c` and `s`, not on the other members of
`L`: the first-char test is per candidate, and the recursion (`llama_grammar_reject_candidates` on the
advanced stacks) again decides per candidate. Call that `rej(s, c)`. The old chain computes
`{c : rej(s1, c) and rej(s2, c) and ...}`, the tokens no stack accepts. The new loop computes
`accepted = {c : not rej(s, c) for some s}` (skipping is allowed exactly when `rej(s, c)` is already known to
be true), and masks the complement. Same set. Only the *order* of the reject list changes, and
`apply_impl` only uses the set. `llama_grammar_reject_candidates_for_stack` itself is untouched, which
matters because `tests/test-llama-grammar.cpp:375-393` checks the order of its output.

**Why it did not work.** (Measured against 0001, before 0002/0003 existed.) The idea was that each stack's sweep was expensive because it *wrote* a copy of
~152k rejected candidates. Measured, the writes were not the problem: P2 still *reads*, for every stack, the
whole 152k x 32-byte candidate array plus one code point per token (~9 MB), and those reads set the
~2.3 ms per-stack cost. P2 removed the cheap sequential writes, kept the expensive reads, and added a second
`llama_grammar_match_char` call for every candidate that passes. Lesson: the Phase 2 profile was of the
*baseline*; after P1 changed the memory layout, the bottleneck had to be re-measured before designing P2.

### Rejected: P2b, scan compact first-code-point arrays per stack

A follow-up to P2: per call, build `first_chr[k]` (4 bytes) and `accepted[k]` (1 byte) for every candidate
and scan only those per stack, with a 128-entry ASCII table, so a stack reads ~0.75 MB instead of ~9 MB.
It passed every correctness gate but was 5% slower than 0001 on typical grammars and not significantly
better at p99: the scan was not the bottleneck either. For the many candidates that *do* pass the first
char (tokens starting with a space at a whitespace position, all of them for string contents) the full
check still runs. Its one real advantage, never making the big per-stack lists, was later obtained more
cheaply by 0003.

### Rejected: skip advance when nothing matched; cheaper `is_eog`

Both correct, both within +-1% of 0001 (0.999x and 1.002x). `llama_grammar_advance_stack` and the
per-token `std::set` lookup were each under 2% of the time, so no change there could reach 3%.

### Lesson from the rejections

The Phase 2 profile was of the *baseline*. Each kept patch changed what the bottleneck was (0001 removed
the allocator storm and exposed the per-stack lists). Ideas designed against an old profile (P2) failed; the
fix that worked (0003) came from measuring the patched code in a new situation (a fresh process).

## Part 3. Interview questions

Numbers below are from `results/REPORT.md` (Qwen2.5 151,936-token vocab, this VM). Where a number is a
headline, say what it measures: **mask computation**, not end-to-end tokens/s.

**1. In one minute: what did you do and what came out of it?**
I built a record/replay benchmark that isolates llama.cpp's GBNF mask computation from model inference
(206 grammars: the 8 built-in ones plus 198 JSONSchemaBench schemas), profiled it, and wrote three small
patches. Together they make the full-vocab mask about FINAL_SPEEDUP faster per grammar (median per-token
cost FINAL_MEDIAN, p99 FINAL_P99), with masks bit-identical to upstream on every step of every trace. On the
default sampling path the end-to-end gain is about 2%, because the full mask only runs when the sampled token is
rejected; the big win is for code paths that mask every step.

**2. Why do bit-identical masks prove correctness?**
The grammar sampler's only output is the mask: which tokens get logit -inf. If, for the same grammar state and
the same candidate tokens, two builds produce the same mask, the rest of sampling cannot tell them apart. The
state after accepting a token comes from `llama_grammar_accept_impl`, which my patches don't change, and
because masks are equal the replay accepts the same tokens, so the states stay equal step after step. I hash
the mask (FNV-1a over the reject bit of every token) at every step of 44,650 recorded steps (two vocabs), and
compare them. It is a proof *for those inputs*; to cover inputs the traces don't hit, I added random valid
walks through every built-in grammar plus edge-case grammars (partial UTF-8 across tokens, byte-fallback
tokens, negated classes, any-char, empty stacks, token elements), and scripted cases for each edge.

**3. What does a hash collision mean for that argument?**
A collision could hide a difference. For one step whose masks differ, a 64-bit hash collides with
probability about 2^-64 (5e-20); over ~60k compared steps that is about 3e-15. I also record the count of
allowed tokens next to each hash, so a difference would have to collide in both. For a stronger claim one would store the full masks (152k bits = 19 KB per step).

**4. Why is the mask so expensive?**
Every step it walks the whole vocab: it decodes every token's text to code points and runs every token through
every live parse stack. Cost per step is roughly 16 ms + 2.3 ms per live stack at baseline. Most steps have 1-4
stacks; JSON objects with many optional properties create hundreds (one stack per remaining property).

**5. Walk me through patch 0001.**
The baseline built one `std::vector<uint32_t>` per token per step (152k allocations + frees). I changed the
decoder to append into a caller's buffer, reserve one buffer for all tokens (upper bound: bytes + 1 per
token, because every code point consumes at least one byte), and point the candidates into it. An assert checks
the buffer never reallocated, so the raw pointers stay valid. Same decoder, same values, different storage.

**6. How is that different from PR #6811, which was closed?**
#6811 cached decoded code points *across steps*, which costs vocab-sized memory forever, needs locking when
several slots share a vocab, and was closed because the end-to-end gain disappeared after the resampling fix.
0001 keeps nothing between calls: no state, no locking, and the transient memory goes *down* (4.5 MB in one
block instead of 11.6 MB in 152k blocks).

**7. You say end-to-end gains are ~2%. So why does this matter?**
On the default path (`common_sampler_sample`) the full mask only runs when the chain's sampled token is
rejected (5% of steps in my runs, 0-64% per grammar). There grammar is ~6% of generation time, so even a much
faster mask gives ~2%. But the full mask runs every step for `grammar_first = true` (the `llama-speculative`
example) and for any application that puts the grammar sampler in a `llama_sampler` chain; there it was 131%
of the model's decode time with this 0.5B model, so the mask speedup translates almost directly. Also, each
rejected step on the default path is a latency spike of one full mask.

**8. Why not just use llguidance?**
It is much faster (about 0.4 ms vs ~20-30 ms per mask here) and it is already an optional backend. But it is a
Rust dependency, it rejects some schemas (36 of 198 here: `oneOf`, unknown formats), it has different
whitespace rules (39 schemas diverged on our traces), and many users build without it. Making the default
engine faster helps everyone without new dependencies; porting llguidance's ideas (a token trie, an Earley
parser) would be a rewrite, not a patch, and llama.cpp prefers small, reviewable changes.

**9. How did you handle VM noise?**
Absolute numbers drifted ~20% over an hour on this VM. So: every comparison interleaves builds in the same
session (A,B then B,A), compares run *i* of A with run *i* of B per grammar (paired), and reports the
geometric-mean per-grammar speedup with a bootstrap 95% CI over grammars, plus medians and the min..max over
runs. Callgrind instruction counts back up the timing, because they don't depend on noise.

**10. Your Phase 2 profile predicted P2 would cut p99 by 40-60%. It made things slower. What happened?**
I designed P2 against the baseline profile, but P1 had already changed the memory behaviour. P2 removed the
copying of reject lists, but the cost was in reading the candidate list for every stack, which P2 kept, and it
added a second match per surviving candidate. I measured it, it was 2.4% slower, so I rejected it and kept it
documented. A follow-up (P2b) that scanned compact arrays also lost on typical grammars.

**11. Tell me about the cold-start regression.**
After the VM rebooted, the 600-stack schema replayed alone in a fresh process was 60% slower with my patches.
perf showed a quarter of the time in kernel page-fault handling, and `perf stat` showed 21M faults vs 3.4M.
Running the same grammar after another one in the same process removed it, and so did raising glibc's
malloc thresholds with `GLIBC_TUNABLES`. That isolated the cause: each stack allocated a new 4.8 MB reject list,
which a cold glibc serves with mmap and the kernel has to fault in. P1 had removed the small allocations that
masked this. The fix (0003) reuses two lists across stacks. The lesson: measure the cold case too, because one
generation per process (llama-cli) is cold.

**12. What trade-offs did you reject?**
A persistent code-point cache (more memory, locking, the #6811 history); P2/P2b (more code, slower on typical
grammars); a cheaper `is_eog` (needed a new vocab accessor for no measurable gain); and `mallopt`-style
allocator tuning (fixes the symptom process-wide, not llama.cpp's business). I also didn't touch the public
API or the order of `llama_grammar_reject_candidates_for_stack`'s output, because a test checks it.

**13. How do you know the ASCII table (0002) isn't noise? It's only ~3.5%.**
Two separate A/B sessions on two different kernels gave +3.6% [2.8, 4.6] and +3.3% [2.2, 4.5] per grammar, paired
over 206 grammars, and total time -3.1% and -4.2%. Instruction counts also drop. It is small but consistent, and
it is 13 lines with an obvious correctness argument (a pure function, tabulated).

**14. What did you learn about the upstream PR #28371?**
I applied it read-only as a third arm. At its head, its cache keys keep raw pointers into buffers that are freed
after each step; valgrind showed thousands of invalid reads and my mask check showed wrong masks and crashes.
Its previous commit, which clears the cache every step, is correct but ~4% slower than baseline on this
workload, because its memo only engages for candidate lists of at most 8 entries, which are rare here. I
didn't post anything upstream; whether to tell the author is a human decision.

**15. If you had another week, what would you do?**
Share work across stacks that have the same top element (hundreds of stacks at one JSON position often expect
the same char), so a token is checked once per distinct top element rather than once per stack; measure the
default sampling path in llama-server with real request mixes; and test on ARM and on other allocators
(jemalloc, mimalloc), since 0003's effect depends on glibc behaviour.
