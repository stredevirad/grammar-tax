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
- with `grammar_first = true` (draft sampling in speculative decoding) the full mask runs every
  step.

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

(after Phase 3)

## Part 3. Interview questions

(after Phase 5)
