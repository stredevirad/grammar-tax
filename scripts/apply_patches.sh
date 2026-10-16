#!/usr/bin/env bash
# Create (or reset) a separate worktree of the pinned llama.cpp submodule and apply patches to it.
# The submodule checkout itself is never modified.
#
# usage: scripts/apply_patches.sh [DEST=build/src-patched] [PATCH...]
#   with no PATCH arguments, applies every patches/*.patch in name order; PATCH = none applies nothing.
set -euo pipefail
cd "$(dirname "$0")/.."
DEST=${1:-build/src-patched}
shift || true
SUB=third_party/llama.cpp
PIN=$(git ls-tree HEAD "$SUB" | awk '{print $3}')
[ -n "$PIN" ] || { echo "cannot read pinned submodule SHA" >&2; exit 1; }

if [ -d "$DEST" ]; then
    git -C "$DEST" reset -q --hard "$PIN"
    git -C "$DEST" clean -qfd
else
    git -C "$SUB" worktree add -q --detach "$(realpath -m "$DEST")" "$PIN"
fi

if [ "${1:-}" = none ]; then
    PATCHES=()
elif [ $# -gt 0 ]; then
    PATCHES=("$@")
else
    shopt -s nullglob
    PATCHES=(patches/*.patch)
fi
for p in "${PATCHES[@]}"; do
    git -C "$DEST" apply --check "$(realpath "$p")"
    git -C "$DEST" apply "$(realpath "$p")"
    echo "applied $p"
done
echo "$DEST at $PIN with ${#PATCHES[@]} patch(es)"
