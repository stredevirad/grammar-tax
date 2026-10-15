#!/usr/bin/env bash
# Download the two models used for recording traces and vocab-only replay.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p models
[ -s models/qwen2.5-0.5b-instruct-q4_k_m.gguf ] || curl -fL -o models/qwen2.5-0.5b-instruct-q4_k_m.gguf \
    https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF/resolve/main/qwen2.5-0.5b-instruct-q4_k_m.gguf
[ -s models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf ] || curl -fL -o models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf \
    https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
cd models && sha256sum -c ../scripts/models.sha256
