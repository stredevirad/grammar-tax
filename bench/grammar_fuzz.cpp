// Differential test: random valid walks through grammars, hashing the full-vocab mask at every step.
// Two builds whose masks are identical produce identical walks and identical output, so comparing the
// --out files of a baseline and a patched build checks the mask on every visited state.
//
// usage: grammar_fuzz --vocab model.gguf --manifest list.tsv --out hashes.tsv [--coverage cov.jsonl]
//                     [--walks 16] [--steps 48] [--seed 1]
// manifest: name <TAB> grammar.gbnf (extra columns ignored)
//
// Token choice per step (deterministic given the masks): EOG with probability 1/8 if allowed (ends the
// walk), otherwise one of: a token whose text ends inside a UTF-8 sequence (forces partial UTF-8 into the
// next step), a token with non-ASCII bytes (multibyte / byte-fallback), or any allowed token.

#include "llama.h"
#include "llama-grammar.h"
#include "llama-vocab.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

static std::string read_file(const std::string & path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void log_quiet(ggml_log_level level, const char * text, void * /*user*/) {
    if (level >= GGML_LOG_LEVEL_ERROR) {
        fputs(text, stderr);
    }
}

// true if the bytes end inside a UTF-8 sequence when decoded from a clean state
static bool ends_partial(const std::string & s) {
    size_t i = 0;
    while (i < s.size()) {
        const uint8_t b = (uint8_t) s[i];
        size_t len = 1;
        if      ((b & 0xE0) == 0xC0) len = 2;
        else if ((b & 0xF0) == 0xE0) len = 3;
        else if ((b & 0xF8) == 0xF0) len = 4;
        if (i + len > s.size()) {
            return true;
        }
        i += len;
    }
    return false;
}

static bool has_non_ascii(const std::string & s) {
    for (char c : s) {
        if ((uint8_t) c >= 0x80) {
            return true;
        }
    }
    return false;
}

static uint64_t fnv1a(const std::string & s) {
    uint64_t h = 1469598103934665603ULL;
    for (char c : s) { h ^= (uint8_t) c; h *= 1099511628211ULL; }
    return h;
}

enum top_kind { TK_CHAR, TK_RANGE, TK_ALT, TK_NOT, TK_ANY, TK_TOKEN, TK_TOKEN_NOT, TK_EMPTY, TK_COUNT };
static const char * top_kind_name[TK_COUNT] = { "char", "range", "alt", "not", "any", "token", "token_not", "empty_stack" };

static void classify_top(const llama_grammar_stack & st, bool * seen) {
    if (st.empty()) { seen[TK_EMPTY] = true; return; }
    const llama_grammar_element * pos = st.back();
    switch (pos->type) {
        case LLAMA_GRETYPE_TOKEN:     seen[TK_TOKEN] = true;     return;
        case LLAMA_GRETYPE_TOKEN_NOT: seen[TK_TOKEN_NOT] = true; return;
        case LLAMA_GRETYPE_CHAR_ANY:  seen[TK_ANY] = true;       break;
        case LLAMA_GRETYPE_CHAR_NOT:  seen[TK_NOT] = true;       break;
        default:                      seen[TK_CHAR] = true;      break;
    }
    // scan the rest of the char class
    const llama_grammar_element * p = pos + 1;
    while (p->type == LLAMA_GRETYPE_CHAR_RNG_UPPER || p->type == LLAMA_GRETYPE_CHAR_ALT || p->type == LLAMA_GRETYPE_CHAR_ANY) {
        if (p->type == LLAMA_GRETYPE_CHAR_RNG_UPPER) seen[TK_RANGE] = true;
        if (p->type == LLAMA_GRETYPE_CHAR_ALT)       seen[TK_ALT] = true;
        if (p->type == LLAMA_GRETYPE_CHAR_ANY)       seen[TK_ANY] = true;
        p++;
    }
}

int main(int argc, char ** argv) {
    std::string vocab_path, manifest_path, out_path, cov_path;
    int walks = 16, steps = 48;
    uint64_t seed = 1;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string a = argv[i];
        if      (a == "--vocab")    vocab_path    = argv[i + 1];
        else if (a == "--manifest") manifest_path = argv[i + 1];
        else if (a == "--out")      out_path      = argv[i + 1];
        else if (a == "--coverage") cov_path      = argv[i + 1];
        else if (a == "--walks")    walks         = std::atoi(argv[i + 1]);
        else if (a == "--steps")    steps         = std::atoi(argv[i + 1]);
        else if (a == "--seed")     seed          = std::strtoull(argv[i + 1], nullptr, 10);
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (vocab_path.empty() || manifest_path.empty() || out_path.empty()) {
        fprintf(stderr, "usage: %s --vocab model.gguf --manifest list.tsv --out hashes.tsv [--coverage cov.jsonl] [--walks N] [--steps N] [--seed S]\n", argv[0]);
        return 2;
    }

    llama_log_set(log_quiet, nullptr);
    llama_backend_init();
    llama_model_params mparams = llama_model_default_params();
    mparams.vocab_only = true;
    llama_model * model = llama_model_load_from_file(vocab_path.c_str(), mparams);
    if (!model) { fprintf(stderr, "failed to load vocab\n"); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    const int n_vocab = llama_vocab_n_tokens(vocab);

    // per-token categories
    std::vector<uint8_t> is_partial(n_vocab), is_non_ascii(n_vocab), is_eog(n_vocab), is_byte1(n_vocab);
    int n_empty_piece = 0, n_eog = 0, n_partial = 0, n_byte1 = 0;
    for (int id = 0; id < n_vocab; id++) {
        const std::string & piece = vocab->token_to_piece(id);
        is_eog[id]       = vocab->is_eog(id) ? 1 : 0;
        is_partial[id]   = ends_partial(piece) ? 1 : 0;
        is_non_ascii[id] = has_non_ascii(piece) ? 1 : 0;
        is_byte1[id]     = (piece.size() == 1 && (uint8_t) piece[0] >= 0x80) ? 1 : 0;
        n_empty_piece   += (piece.empty() || piece[0] == 0) ? 1 : 0;
        n_eog           += is_eog[id];
        n_partial       += is_partial[id];
        n_byte1         += is_byte1[id];
    }

    FILE * fo = fopen(out_path.c_str(), "w");
    FILE * fc = cov_path.empty() ? nullptr : fopen(cov_path.c_str(), "w");
    if (fc) {
        fprintf(fc, "{\"vocab\":\"%s\",\"n_vocab\":%d,\"n_eog\":%d,\"n_empty_piece\":%d,\"n_partial_end\":%d,\"n_single_high_byte\":%d}\n",
                vocab_path.c_str(), n_vocab, n_eog, n_empty_piece, n_partial, n_byte1);
    }

    std::ifstream mf(manifest_path);
    std::string line;
    std::vector<llama_token_data> cur(n_vocab);
    std::vector<int> allowed, pick_partial, pick_non_ascii, allowed_eog;
    int rc = 0;

    while (std::getline(mf, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string name, gpath;
        std::getline(ls, name, '\t');
        std::getline(ls, gpath, '\t');
        const std::string gstr = read_file(gpath);

        long c_steps = 0, c_partial_in = 0, c_eog_allowed = 0, c_eog_taken = 0;
        long c_pick_partial = 0, c_pick_non_ascii = 0, c_pick_byte1 = 0, c_dead = 0;
        long c_top[TK_COUNT] = {0};

        for (int w = 0; w < walks; w++) {
            std::mt19937_64 rng(seed ^ fnv1a(name) ^ (0x9E3779B97F4A7C15ULL * (uint64_t) (w + 1)));
            llama_grammar * grammar = llama_grammar_init_impl(vocab, gstr.c_str(), "root", false, nullptr, 0, nullptr, 0);
            if (!grammar) { fprintf(stderr, "%s: grammar init failed\n", name.c_str()); rc = 1; break; }

            for (int step = 0; step < steps; step++) {
                bool seen[TK_COUNT] = {false};
                for (const auto & st : grammar->stacks) classify_top(st, seen);
                for (int k = 0; k < TK_COUNT; k++) c_top[k] += seen[k] ? 1 : 0;
                const int partial_in = grammar->partial_utf8.n_remain;
                c_partial_in += partial_in > 0 ? 1 : 0;

                for (int id = 0; id < n_vocab; id++) cur[id] = { id, 0.0f, 0.0f };
                llama_token_data_array cur_p = { cur.data(), (size_t) n_vocab, -1, false };
                llama_grammar_apply_impl(*grammar, &cur_p);

                uint64_t h = 1469598103934665603ULL;
                allowed.clear(); pick_partial.clear(); pick_non_ascii.clear(); allowed_eog.clear();
                for (int id = 0; id < n_vocab; id++) {
                    const uint8_t r = std::isinf(cur[id].logit) ? 1 : 0;
                    h ^= r; h *= 1099511628211ULL;
                    if (r) continue;
                    if (is_eog[id]) { allowed_eog.push_back(id); continue; }
                    allowed.push_back(id);
                    if (is_partial[id])   pick_partial.push_back(id);
                    if (is_non_ascii[id]) pick_non_ascii.push_back(id);
                }
                c_steps++;
                c_eog_allowed += allowed_eog.empty() ? 0 : 1;

                llama_token tok = -1;
                const uint64_t r = rng();
                if (!allowed_eog.empty() && (allowed.empty() || r % 8 == 0)) {
                    tok = allowed_eog[(r >> 8) % allowed_eog.size()];
                } else if (!allowed.empty()) {
                    const std::vector<int> * from = &allowed;
                    if (r % 3 == 0 && !pick_partial.empty())   from = &pick_partial;
                    if (r % 3 == 1 && !pick_non_ascii.empty()) from = &pick_non_ascii;
                    tok = (*from)[(r >> 8) % from->size()];
                }
                fprintf(fo, "%s\t%d\t%d\t%zu\t%016llx\t%d\t%zu\t%d\n", name.c_str(), w, step, allowed.size() + allowed_eog.size(),
                        (unsigned long long) h, partial_in, grammar->stacks.size(), tok);
                if (tok < 0) { c_dead++; break; }

                c_pick_partial   += is_partial[tok];
                c_pick_non_ascii += is_non_ascii[tok];
                c_pick_byte1     += is_byte1[tok];
                try {
                    llama_grammar_accept_impl(*grammar, tok);
                } catch (const std::exception & e) {
                    fprintf(stderr, "%s walk %d step %d: accept failed: %s\n", name.c_str(), w, step, e.what());
                    rc = 1;
                    break;
                }
                if (is_eog[tok]) { c_eog_taken++; break; }
            }
            llama_grammar_free_impl(grammar);
        }
        if (fc) {
            fprintf(fc, "{\"name\":\"%s\",\"steps\":%ld,\"partial_utf8_in\":%ld,\"eog_allowed\":%ld,\"eog_taken\":%ld,"
                        "\"picked_partial_end\":%ld,\"picked_non_ascii\":%ld,\"picked_single_high_byte\":%ld,\"dead_ends\":%ld",
                    name.c_str(), c_steps, c_partial_in, c_eog_allowed, c_eog_taken, c_pick_partial, c_pick_non_ascii, c_pick_byte1, c_dead);
            for (int k = 0; k < TK_COUNT; k++) fprintf(fc, ",\"top_%s\":%ld", top_kind_name[k], c_top[k]);
            fprintf(fc, "}\n");
            fflush(fc);
        }
        fflush(fo);
    }
    fclose(fo);
    if (fc) fclose(fc);
    llama_model_free(model);
    llama_backend_free();
    return rc;
}
