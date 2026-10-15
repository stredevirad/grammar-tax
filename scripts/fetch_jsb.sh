#!/usr/bin/env bash
# Download JSONSchemaBench test splits (pinned dataset revision) into data/raw/jsb/.
set -euo pipefail
cd "$(dirname "$0")/.."
REV=5bd0f4640badc6f3f02df796421d21cb0ca0b141
mkdir -p data/raw/jsb
for s in Github_trivial Github_easy Github_medium Github_hard Github_ultra Glaiveai2K JsonSchemaStore Kubernetes Snowplow WashingtonPost; do
    [ -s data/raw/jsb/$s.parquet ] || curl -fsSL -o data/raw/jsb/$s.parquet \
        "https://huggingface.co/datasets/epfl-dlab/JSONSchemaBench/resolve/$REV/$s/test-00000-of-00001.parquet"
done
