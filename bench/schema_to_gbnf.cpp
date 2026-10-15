// Convert one JSON schema file to GBNF with llama.cpp's own json-schema-to-grammar
// (the same entry point used by --json-schema and the server's response_format).
// usage: schema_to_gbnf in.json out.gbnf
// exit 0 on success, 1 on conversion failure (message on stderr).

#include "json-schema-to-grammar.h"
#include "json.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char ** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s in.json out.gbnf\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    std::stringstream ss;
    ss << in.rdbuf();

    std::string gbnf;
    try {
        gbnf = json_schema_to_grammar(common_json::parse(ss.str()));
    } catch (const std::exception & e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    std::ofstream out(argv[2]);
    out << gbnf;
    return 0;
}
