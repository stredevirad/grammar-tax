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
(`scripts/apply_patches.sh`). Numbers quoted here come from `results/REPORT.md`; this part is about
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

### Patch 0002: skip candidates whose first char cannot match the stack

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

**Why it is faster.** Per stack, the old code read ~152k candidate structs, dereferenced 152k code-point
pointers and wrote ~152k structs. The new code reads the same structs and one code point each, but writes
only the few candidates that pass, and skips the deep recursion for nothing extra. The per-stack cost goes
from "copy the vocab" to "scan the vocab".

## Part 3. Interview questions

(after Phase 5)
