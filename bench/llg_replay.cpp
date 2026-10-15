// Reference ceiling: replay the same traces through llguidance (llama.cpp's optional
// -DLLAMA_LLGUIDANCE=ON backend) using the original JSON schema, and time mask + accept.
// llguidance has its own JSON-schema semantics, so masks are not comparable to GBNF masks;
// if a recorded token is not allowed by llguidance, replay of that trace stops there.
//
// usage: llg_replay --vocab model.gguf --manifest llg.tsv --json out.jsonl
// manifest: name <TAB> schema.json <TAB> trace.txt

#include "llama.h"
#include "sampling.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

static double now_us() {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static double percentile(std::vector<double> v, double p) {
    if (v.empty()) {
        return 0.0;
    }
    std::sort(v.begin(), v.end());
    const double idx = p * (v.size() - 1);
    const size_t lo = (size_t) std::floor(idx);
    const size_t hi = (size_t) std::ceil(idx);
    return v[lo] + (v[hi] - v[lo]) * (idx - lo);
}

static void log_quiet(ggml_log_level level, const char * text, void * /*user*/) {
    if (level >= GGML_LOG_LEVEL_ERROR) {
        fputs(text, stderr);
    }
}

int main(int argc, char ** argv) {
    std::string vocab_path, manifest_path, json_path;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string a = argv[i];
        if      (a == "--vocab")    vocab_path    = argv[i + 1];
        else if (a == "--manifest") manifest_path = argv[i + 1];
        else if (a == "--json")     json_path     = argv[i + 1];
    }
    if (vocab_path.empty() || manifest_path.empty() || json_path.empty()) {
        fprintf(stderr, "usage: %s --vocab model.gguf --manifest llg.tsv --json out.jsonl\n", argv[0]);
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

    FILE * fj = fopen(json_path.c_str(), "w");
    std::ifstream mf(manifest_path);
    std::string line;
    std::vector<llama_token_data> cur(n_vocab);

    while (std::getline(mf, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string name, spath, tpath;
        std::getline(ls, name, '\t');
        std::getline(ls, spath, '\t');
        std::getline(ls, tpath, '\t');

        std::vector<llama_token> trace;
        {
            std::ifstream tf(tpath);
            std::string tl;
            while (std::getline(tf, tl)) {
                if (tl.empty() || tl[0] == '#') continue;
                std::istringstream ss(tl);
                long v;
                while (ss >> v) trace.push_back((llama_token) v);
            }
        }

        const std::string schema = read_file(spath);
        const double ti0 = now_us();
        llama_sampler * smpl = llama_sampler_init_llg(vocab, "json", schema.c_str());
        const double init_us = now_us() - ti0;

        std::vector<double> tok_us;
        std::string status = "ok";
        if (!smpl) {
            status = "init_failed";
        } else {
            for (size_t step = 0; step < trace.size(); step++) {
                for (int id = 0; id < n_vocab; id++) {
                    cur[id] = { id, 0.0f, 0.0f };
                }
                llama_token_data_array cur_p = { cur.data(), (size_t) n_vocab, -1, false };
                const llama_token tok = trace[step];
                const double t0 = now_us();
                llama_sampler_apply(smpl, &cur_p);
                const double t1 = now_us();
                if (step == 0) {
                    // the llama.cpp wrapper keeps a null matcher when llguidance rejects the schema;
                    // such a sampler masks nothing, so treat it as unsupported
                    int n_rejected = 0;
                    for (int id = 0; id < n_vocab; id++) {
                        n_rejected += std::isinf(cur[id].logit) ? 1 : 0;
                    }
                    if (n_rejected == 0) {
                        status = "unsupported";
                        break;
                    }
                }
                if (std::isinf(cur[tok].logit)) {
                    status = "diverged";
                    break;
                }
                llama_sampler_accept(smpl, tok);
                const double t2 = now_us();
                tok_us.push_back(t2 - t0);
                (void) t1;
            }
            llama_sampler_free(smpl);
        }

        double sum = 0.0;
        for (double v : tok_us) sum += v;
        fprintf(fj, "{\"name\":\"%s\",\"status\":\"%s\",\"trace_tokens\":%zu,\"tokens\":%zu,\"init_us\":%.1f,"
                    "\"tok_us_mean\":%.2f,\"tok_us_p50\":%.2f,\"tok_us_p99\":%.2f}\n",
                name.c_str(), status.c_str(), trace.size(), tok_us.size(), init_us,
                tok_us.empty() ? 0.0 : sum / tok_us.size(), percentile(tok_us, 0.5), percentile(tok_us, 0.99));
        fflush(fj);
    }
    fclose(fj);
    llama_model_free(model);
    llama_backend_free();
    return 0;
}
