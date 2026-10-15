// Record one grammar-constrained generation as a list of token ids.
//
// usage: grammar_record -m model.gguf -g grammar.gbnf -p prompt.txt -o trace.txt
//                       [--seed 1234] [--max-tokens 256] [--max-seconds 60] [-t 4] [--mode first|resample]
//
// --mode first (default, used for recording): the grammar mask is applied to the full vocab
// at every step, then top-k 40 / temp 0.7 / seeded dist, so the trace is always grammar-valid.
// --mode resample: mirrors common_sampler_sample(grammar_first = false): sample with the chain,
// check only the chosen token against the grammar, and run the full-vocab mask + resample only
// if it is rejected. Reports how many steps needed the full mask (n_full_mask).
// Output trace format (text):
//   line 1: "# key=value ..." metadata (model, seed, n_prompt, n_gen, complete, timings)
//   line 2: space separated token ids
// Also writes <trace>.out.txt with the generated text, for eyeballing.

#include "llama.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::string read_file(const std::string & path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static double now_ms() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void log_quiet(ggml_log_level level, const char * text, void * /*user*/) {
    if (level >= GGML_LOG_LEVEL_ERROR) {
        fputs(text, stderr);
    }
}

int main(int argc, char ** argv) {
    std::string model_path, grammar_path, prompt_path, out_path;
    uint32_t seed = 1234;
    int max_tokens = 256;
    double max_seconds = 60.0;
    int n_threads = 4;
    bool mode_resample = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(2); } return argv[++i]; };
        if      (a == "-m")            model_path   = next();
        else if (a == "-g")            grammar_path = next();
        else if (a == "-p")            prompt_path  = next();
        else if (a == "-o")            out_path     = next();
        else if (a == "--seed")        seed         = (uint32_t) std::stoul(next());
        else if (a == "--max-tokens")  max_tokens   = std::stoi(next());
        else if (a == "--max-seconds") max_seconds  = std::stod(next());
        else if (a == "-t")            n_threads    = std::stoi(next());
        else if (a == "--mode")        mode_resample = next() == "resample";
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (model_path.empty() || grammar_path.empty() || prompt_path.empty() || out_path.empty()) {
        fprintf(stderr, "usage: %s -m model -g grammar.gbnf -p prompt.txt -o trace.txt\n", argv[0]);
        return 2;
    }

    llama_log_set(log_quiet, nullptr);
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    llama_model * model = llama_model_load_from_file(model_path.c_str(), mparams);
    if (!model) { fprintf(stderr, "failed to load model\n"); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);
    const int n_vocab = llama_vocab_n_tokens(vocab);

    // chat-formatted prompt
    const std::string user_msg = read_file(prompt_path);
    llama_chat_message msgs[2] = {
        { "system", "You are a helpful assistant. Answer only with output that follows the requested format." },
        { "user",   user_msg.c_str() },
    };
    const char * tmpl = llama_model_chat_template(model, nullptr);
    std::vector<char> buf(user_msg.size() * 2 + 4096);
    int n = llama_chat_apply_template(tmpl, msgs, 2, true, buf.data(), (int32_t) buf.size());
    if (n > (int) buf.size()) {
        buf.resize(n + 1);
        n = llama_chat_apply_template(tmpl, msgs, 2, true, buf.data(), (int32_t) buf.size());
    }
    if (n < 0) { fprintf(stderr, "chat template failed\n"); return 1; }
    const std::string prompt(buf.data(), n);

    std::vector<llama_token> prompt_tokens(prompt.size() + 16);
    int n_prompt = llama_tokenize(vocab, prompt.c_str(), (int32_t) prompt.size(), prompt_tokens.data(), (int32_t) prompt_tokens.size(), true, true);
    if (n_prompt < 0) { fprintf(stderr, "tokenize failed\n"); return 1; }
    prompt_tokens.resize(n_prompt);

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx           = n_prompt + max_tokens + 16;
    cparams.n_batch         = 512;
    cparams.n_threads       = n_threads;
    cparams.n_threads_batch = n_threads;
    llama_context * ctx = llama_init_from_model(model, cparams);
    if (!ctx) { fprintf(stderr, "failed to create context\n"); return 1; }

    const std::string grammar_str = read_file(grammar_path);
    llama_sampler * grmr = llama_sampler_init_grammar(vocab, grammar_str.c_str(), "root");
    if (!grmr) { fprintf(stderr, "failed to parse grammar\n"); return 1; }

    llama_sampler * chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(chain, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(chain, llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(chain, llama_sampler_init_dist(seed));

    const double t_start = now_ms();
    double t_decode = 0.0, t_grammar = 0.0, t_prompt = 0.0, t_chain = 0.0;
    int n_full_mask = 0;

    // prompt eval in n_batch chunks
    for (int i = 0; i < n_prompt; i += 512) {
        const int n_eval = std::min(512, n_prompt - i);
        const double t0 = now_ms();
        if (llama_decode(ctx, llama_batch_get_one(prompt_tokens.data() + i, n_eval))) { fprintf(stderr, "decode failed\n"); return 1; }
        t_prompt += now_ms() - t0;
    }

    std::vector<llama_token> out;
    std::string text;
    std::vector<llama_token_data> cur(n_vocab);
    bool complete = false;
    bool timed_out = false;

    for (int step = 0; step < max_tokens; step++) {
        const float * logits = llama_get_logits_ith(ctx, -1);
        for (int id = 0; id < n_vocab; id++) {
            cur[id] = { id, logits[id], 0.0f };
        }
        llama_token_data_array cur_p = { cur.data(), (size_t) n_vocab, -1, false };

        llama_token tok;
        if (!mode_resample) {
            const double t0 = now_ms();
            llama_sampler_apply(grmr, &cur_p);
            t_grammar += now_ms() - t0;
            n_full_mask++;

            const double tc = now_ms();
            llama_sampler_apply(chain, &cur_p);
            t_chain += now_ms() - tc;
            tok = cur_p.data[cur_p.selected].id;
        } else {
            const double tc = now_ms();
            llama_sampler_apply(chain, &cur_p);
            t_chain += now_ms() - tc;
            tok = cur_p.data[cur_p.selected].id;

            llama_token_data single = { tok, 1.0f, 0.0f };
            llama_token_data_array single_p = { &single, 1, -1, false };
            const double t0 = now_ms();
            llama_sampler_apply(grmr, &single_p);
            t_grammar += now_ms() - t0;

            if (single.logit == -INFINITY) {
                for (int id = 0; id < n_vocab; id++) {
                    cur[id] = { id, logits[id], 0.0f };
                }
                cur_p = { cur.data(), (size_t) n_vocab, -1, false };
                const double t4 = now_ms();
                llama_sampler_apply(grmr, &cur_p);
                t_grammar += now_ms() - t4;
                n_full_mask++;

                const double tc2 = now_ms();
                llama_sampler_apply(chain, &cur_p);
                t_chain += now_ms() - tc2;
                tok = cur_p.data[cur_p.selected].id;
            }
        }

        const double t1 = now_ms();
        llama_sampler_accept(grmr, tok);
        t_grammar += now_ms() - t1;
        llama_sampler_accept(chain, tok);

        out.push_back(tok);
        if (llama_vocab_is_eog(vocab, tok)) {
            complete = true;
            break;
        }
        char piece[256];
        const int np = llama_token_to_piece(vocab, tok, piece, sizeof(piece), 0, true);
        if (np > 0) {
            text.append(piece, np);
        }

        if ((now_ms() - t_start) / 1000.0 > max_seconds) {
            timed_out = true;
            break;
        }

        llama_token t = tok;
        const double t2 = now_ms();
        if (llama_decode(ctx, llama_batch_get_one(&t, 1))) { fprintf(stderr, "decode failed\n"); return 1; }
        t_decode += now_ms() - t2;
    }

    const char * model_name = strrchr(model_path.c_str(), '/');
    model_name = model_name ? model_name + 1 : model_path.c_str();

    FILE * f = fopen(out_path.c_str(), "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", out_path.c_str()); return 1; }
    fprintf(f, "# model=%s seed=%u mode=%s n_prompt=%d n_gen=%zu complete=%d timed_out=%d n_full_mask=%d t_prompt_ms=%.1f t_decode_ms=%.1f t_grammar_ms=%.1f t_chain_ms=%.1f\n",
            model_name, seed, mode_resample ? "resample" : "first", n_prompt, out.size(), complete ? 1 : 0, timed_out ? 1 : 0, n_full_mask, t_prompt, t_decode, t_grammar, t_chain);
    for (size_t i = 0; i < out.size(); i++) {
        fprintf(f, "%s%d", i ? " " : "", out[i]);
    }
    fprintf(f, "\n");
    fclose(f);

    std::ofstream(out_path + ".out.txt") << text;

    printf("n_prompt=%d n_gen=%zu complete=%d timed_out=%d n_full_mask=%d decode=%.1fms grammar=%.1fms chain=%.1fms\n",
           n_prompt, out.size(), complete ? 1 : 0, timed_out ? 1 : 0, n_full_mask, t_decode, t_grammar, t_chain);

    llama_sampler_free(chain);
    llama_sampler_free(grmr);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
