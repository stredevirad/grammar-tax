#!/usr/bin/env python3
"""Build the benchmark workload.

(a) every grammars/*.gbnf in the pinned llama.cpp submodule
(b) a stratified sample of JSONSchemaBench test-split schemas (N per subset, fixed seed),
    converted to GBNF with llama.cpp's own json-schema-to-grammar (bench/schema_to_gbnf).

Writes:
  data/schemas/<subset>/<id>.json      sampled schemas
  data/grammars/<name>.gbnf            converted grammars (jsb__*) ; gbnf__* point into the submodule
  data/prompts/<name>.txt              user prompt used when recording the trace
  data/workload.tsv                    name <TAB> grammar path <TAB> trace path (relative to repo root)
  data/conversion_failures.tsv         schemas dropped because conversion failed
Run with the venv python: build/venv/bin/python -I scripts/prepare_workload.py
"""
import argparse
import json
import os
import re
import subprocess
import sys

import pandas as pd

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SUBSETS = ["Github_trivial", "Github_easy", "Github_medium", "Github_hard", "Github_ultra",
           "Glaiveai2K", "JsonSchemaStore", "Kubernetes", "Snowplow", "WashingtonPost"]

GBNF_PROMPTS = {
    "arithmetic": "Write a long arithmetic expression using +, -, *, / and parentheses.",
    "c":          "Write a small C program that prints the first 10 Fibonacci numbers.",
    "chess":      "Write the moves of a chess game in standard algebraic notation, numbered, e.g. 1. e4 e5 2. Nf3 Nc6.",
    "english":    "Write a short paragraph about the history of the printing press.",
    "japanese":   "日本の四季について短い文章を日本語で書いてください。",
    "json":       "Describe a fictional person (name, age, address, hobbies, employment history) as a JSON object.",
    "json_arr":   "List five fictional books with title, author, year and genres as a JSON array of objects.",
    "list":       "Write a shopping list for a week of dinners, one item per line starting with '- '.",
}

SCHEMA_PROMPT = ("Generate one realistic JSON instance that conforms to the following JSON schema. "
                 "Output only the JSON.\n\nSchema:\n{schema}\n")
MAX_SCHEMA_CHARS = 6000


def safe(s):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", s)[:80]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--per-subset", type=int, default=20)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--converter", default=os.path.join(ROOT, "build/bench-base/bin/schema_to_gbnf"))
    args = ap.parse_args()

    os.chdir(ROOT)
    for d in ["data/schemas", "data/grammars", "data/prompts"]:
        os.makedirs(d, exist_ok=True)

    rows = []
    # (a) built-in grammars
    gdir = "third_party/llama.cpp/grammars"
    for fn in sorted(os.listdir(gdir)):
        if not fn.endswith(".gbnf"):
            continue
        base = fn[:-5]
        name = "gbnf__" + base
        with open(f"data/prompts/{name}.txt", "w") as f:
            f.write(GBNF_PROMPTS.get(base, "Write some text."))
        rows.append((name, f"{gdir}/{fn}", f"bench/traces/{{model}}/{name}.txt"))

    # (b) JSONSchemaBench stratified sample
    failures = []
    for subset in SUBSETS:
        df = pd.read_parquet(f"data/raw/jsb/{subset}.parquet")
        k = min(args.per_subset, len(df))
        sample = df.sample(n=k, random_state=args.seed).sort_values("unique_id")
        os.makedirs(f"data/schemas/{subset}", exist_ok=True)
        for _, r in sample.iterrows():
            uid = safe(str(r["unique_id"]))
            name = f"jsb__{subset}__{uid}"
            spath = f"data/schemas/{subset}/{uid}.json"
            with open(spath, "w") as f:
                f.write(r["json_schema"])
            gpath = f"data/grammars/{name}.gbnf"
            p = subprocess.run([args.converter, spath, gpath], capture_output=True, text=True)
            if p.returncode != 0:
                msg = (p.stderr.strip().splitlines() or ["?"])
                failures.append((name, subset, " | ".join(msg)[:300]))
                if os.path.exists(gpath):
                    os.remove(gpath)
                continue
            schema_txt = r["json_schema"]
            try:
                schema_txt = json.dumps(json.loads(schema_txt), indent=1)
            except Exception:
                pass
            if len(schema_txt) > MAX_SCHEMA_CHARS:
                schema_txt = schema_txt[:MAX_SCHEMA_CHARS] + "\n... (truncated)"
            with open(f"data/prompts/{name}.txt", "w") as f:
                f.write(SCHEMA_PROMPT.format(schema=schema_txt))
            rows.append((name, gpath, f"bench/traces/{{model}}/{name}.txt"))

    # JSONSchemaBench has a few duplicate unique_ids (JsonSchemaStore 'project-1' x3 in our sample).
    # Same name means the later schema overwrote the earlier files; keep one manifest row per name.
    seen = set()
    rows = [r for r in rows if not (r[0] in seen or seen.add(r[0]))]

    with open("data/workload.tsv", "w") as f:
        f.write("# name\tgrammar\ttrace ({model} is replaced by the model tag)\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    with open("data/conversion_failures.tsv", "w") as f:
        f.write("name\tsubset\terror\n")
        for r in failures:
            f.write("\t".join(x.replace("\t", " ").replace("\n", " ") for x in r) + "\n")

    n_jsb = sum(1 for r in rows if r[0].startswith("jsb__"))
    print(f"gbnf grammars: {len(rows) - n_jsb}, schemas converted: {n_jsb}, conversion failures: {len(failures)}")


if __name__ == "__main__":
    sys.exit(main())
