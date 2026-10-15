// Replay recorded token traces through llama.cpp's grammar engine and time only the
// grammar work: for every recorded token, compute the full mask over the entire vocab
// (llama_grammar_apply_impl), then accept the token (llama_grammar_accept_impl).
// Only the vocab is loaded (vocab_only), so there is no model-inference noise.
//
// usage: grammar_replay --vocab model.gguf --manifest workload.tsv [--json out.jsonl]
//                       [--check masks.tsv] [--tokens tokens.tsv] [--filter substr] [--repeat N]
//                       [--max-steps N]
//
// manifest: one line per grammar, tab separated: name <TAB> grammar.gbnf <TAB> trace.txt
// --json   : one JSON object per grammar (timings, stacks, tokens)
// --check  : per-step mask hash (FNV-1a over the reject bitmap) for bit-exact A/B comparison
// --tokens : one row per replayed token: name, step, mask_us, accept_us, n_stacks, n_allowed
// --max-steps : replay only the first N tokens of each trace (used to keep callgrind runs short)
// --repeat : replay each grammar N times in-process, report the run with the lowest total
//            (all repeats must produce identical masks)

#include "llama.h"
#include "llama-grammar.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
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

static bool read_trace(const std::string & path, std::vector<llama_token> & out) {
    std::ifstream f(path);
    if (!f) {
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        long v;
        while (ss >> v) {
            out.push_back((llama_token) v);
        }
    }
    return true;
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

static std::string json_escape(const std::string & s) {
    std::string r;
    for (char c : s) {
        if (c == '"' || c == '\\') { r += '\\'; r += c; }
        else if ((unsigned char) c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); r += b; }
        else r += c;
    }
    return r;
}

static void log_quiet(ggml_log_level level, const char * text, void * /*user*/) {
    if (level >= GGML_LOG_LEVEL_ERROR) {
        fputs(text, stderr);
    }
}

struct replay_result {
    bool                  ok = true;
    std::string           error;
    std::vector<double>   mask_us;
    std::vector<double>   accept_us;
    std::vector<uint64_t> hashes;
    std::vector<int>      n_allowed;
    std::vector<size_t>   n_stacks;    // live stacks before each mask
    size_t                peak_stacks = 0;
    double                total_us    = 0.0;
};

static replay_result replay_one(const llama_vocab * vocab, const std::string & grammar_str, const std::vector<llama_token> & trace, size_t max_steps) {
    replay_result res;
    const int n_vocab = llama_vocab_n_tokens(vocab);

    llama_grammar * grammar = llama_grammar_init_impl(vocab, grammar_str.c_str(), "root", false, nullptr, 0, nullptr, 0);
    if (!grammar) {
        res.ok = false;
        res.error = "grammar init failed";
        return res;
    }

    std::vector<llama_token_data> cur(n_vocab);
    std::vector<uint8_t> rejected(n_vocab);
    res.peak_stacks = grammar->stacks.size();

    for (size_t step = 0; step < trace.size() && step < max_steps; step++) {
        for (int id = 0; id < n_vocab; id++) {
            cur[id] = { id, 0.0f, 0.0f };
        }
        llama_token_data_array cur_p = { cur.data(), (size_t) n_vocab, -1, false };

        const size_t n_stacks_step = grammar->stacks.size();
        const double t0 = now_us();
        llama_grammar_apply_impl(*grammar, &cur_p);
        const double t1 = now_us();

        // FNV-1a over the reject bitmap (cur_p keeps vocab order, apply only writes logits)
        uint64_t h = 1469598103934665603ULL;
        int n_allowed = 0;
        for (int id = 0; id < n_vocab; id++) {
            const uint8_t r = std::isinf(cur[id].logit) ? 1 : 0;
            rejected[id] = r;
            n_allowed += 1 - r;
            h ^= r;
            h *= 1099511628211ULL;
        }

        const llama_token tok = trace[step];
        if (tok < 0 || tok >= n_vocab || rejected[tok]) {
            res.ok = false;
            res.error = "trace token " + std::to_string(tok) + " rejected at step " + std::to_string(step);
            res.mask_us.push_back(t1 - t0);
            res.hashes.push_back(h);
            res.n_allowed.push_back(n_allowed);
            break;
        }

        double t3;
        const double t2 = now_us();
        try {
            llama_grammar_accept_impl(*grammar, tok);
            t3 = now_us();
        } catch (const std::exception & e) {
            res.ok = false;
            res.error = std::string("accept failed: ") + e.what();
            break;
        }

        res.mask_us.push_back(t1 - t0);
        res.accept_us.push_back(t3 - t2);
        res.hashes.push_back(h);
        res.n_allowed.push_back(n_allowed);
        res.n_stacks.push_back(n_stacks_step);
        res.total_us += (t1 - t0) + (t3 - t2);
        res.peak_stacks = std::max(res.peak_stacks, grammar->stacks.size());
    }

    llama_grammar_free_impl(grammar);
    return res;
}

int main(int argc, char ** argv) {
    std::string vocab_path, manifest_path, json_path, check_path, tokens_path, filter;
    int repeat = 1;
    size_t max_steps = SIZE_MAX;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(2); } return argv[++i]; };
        if      (a == "--vocab")    vocab_path    = next();
        else if (a == "--manifest") manifest_path = next();
        else if (a == "--json")     json_path     = next();
        else if (a == "--check")    check_path    = next();
        else if (a == "--tokens")   tokens_path   = next();
        else if (a == "--filter")   filter        = next();
        else if (a == "--repeat")   repeat        = std::max(1, std::stoi(next()));
        else if (a == "--max-steps") max_steps    = (size_t) std::stoul(next());
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (vocab_path.empty() || manifest_path.empty()) {
        fprintf(stderr, "usage: %s --vocab model.gguf --manifest workload.tsv [--json out.jsonl] [--check masks.tsv] [--filter s] [--repeat N]\n", argv[0]);
        return 2;
    }

    llama_log_set(log_quiet, nullptr);
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.vocab_only = true;
    llama_model * model = llama_model_load_from_file(vocab_path.c_str(), mparams);
    if (!model) { fprintf(stderr, "failed to load vocab\n"); return 1; }
    const llama_vocab * vocab = llama_model_get_vocab(model);

    FILE * fj = json_path.empty()  ? nullptr : fopen(json_path.c_str(), "w");
    FILE * fc = check_path.empty() ? nullptr : fopen(check_path.c_str(), "w");
    FILE * ft = tokens_path.empty() ? nullptr : fopen(tokens_path.c_str(), "w");

    std::ifstream mf(manifest_path);
    std::string line;
    int n_fail = 0;
    while (std::getline(mf, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string name, gpath, tpath;
        std::getline(ls, name, '\t');
        std::getline(ls, gpath, '\t');
        std::getline(ls, tpath, '\t');
        if (!filter.empty() && name.find(filter) == std::string::npos) {
            continue;
        }

        const std::string grammar_str = read_file(gpath);
        std::vector<llama_token> trace;
        if (!read_trace(tpath, trace)) {
            fprintf(stderr, "%s: cannot read trace %s\n", name.c_str(), tpath.c_str());
            n_fail++;
            continue;
        }

        replay_result best;
        for (int r = 0; r < repeat; r++) {
            replay_result res = replay_one(vocab, grammar_str, trace, max_steps);
            if (r > 0 && res.hashes != best.hashes) {
                fprintf(stderr, "%s: masks differ between repeats\n", name.c_str());
                return 1;
            }
            if (r == 0 || res.total_us < best.total_us) {
                best = std::move(res);
            }
        }

        std::vector<double> tok_us(best.accept_us.size());
        for (size_t i = 0; i < tok_us.size(); i++) {
            tok_us[i] = best.mask_us[i] + best.accept_us[i];
        }
        const size_t n = tok_us.size();
        double sum_mask = 0.0, sum_acc = 0.0;
        for (size_t i = 0; i < n; i++) { sum_mask += best.mask_us[i]; sum_acc += best.accept_us[i]; }

        if (!best.ok) {
            n_fail++;
            fprintf(stderr, "%s: FAIL %s\n", name.c_str(), best.error.c_str());
        }
        if (fj) {
            fprintf(fj,
                "{\"name\":\"%s\",\"ok\":%s,\"error\":\"%s\",\"tokens\":%zu,\"total_us\":%.1f,"
                "\"mask_us_total\":%.1f,\"accept_us_total\":%.1f,"
                "\"tok_us_mean\":%.2f,\"tok_us_p50\":%.2f,\"tok_us_p99\":%.2f,\"tok_us_max\":%.2f,"
                "\"mask_us_mean\":%.2f,\"mask_us_p50\":%.2f,\"mask_us_p99\":%.2f,"
                "\"accept_us_mean\":%.2f,\"peak_stacks\":%zu}\n",
                json_escape(name).c_str(), best.ok ? "true" : "false", json_escape(best.error).c_str(), n, best.total_us,
                sum_mask, sum_acc,
                n ? best.total_us / n : 0.0, percentile(tok_us, 0.5), percentile(tok_us, 0.99), n ? *std::max_element(tok_us.begin(), tok_us.end()) : 0.0,
                n ? sum_mask / n : 0.0, percentile(best.mask_us, 0.5), percentile(best.mask_us, 0.99),
                n ? sum_acc / n : 0.0, best.peak_stacks);
            fflush(fj);
        }
        if (ft) {
            for (size_t i = 0; i < best.accept_us.size(); i++) {
                fprintf(ft, "%s\t%zu\t%.2f\t%.2f\t%zu\t%d\n", name.c_str(), i, best.mask_us[i], best.accept_us[i], best.n_stacks[i], best.n_allowed[i]);
            }
        }
        if (fc) {
            for (size_t i = 0; i < best.hashes.size(); i++) {
                fprintf(fc, "%s\t%zu\t%d\t%016llx\n", name.c_str(), i, best.n_allowed[i], (unsigned long long) best.hashes[i]);
            }
        }
    }

    if (ft) fclose(ft);
    if (fj) fclose(fj);
    if (fc) fclose(fc);
    llama_model_free(model);
    llama_backend_free();
    return n_fail ? 3 : 0;
}
