#!/usr/bin/env bash
# Write patches/NNNN-name.patch (git format-patch layout) from the uncommitted diff of a worktree,
# relative to BASE_REF (default: the pinned submodule SHA). No commit is created anywhere.
# usage: scripts/make_patch.sh WORKTREE NNNN short-name "subject" [BASE_REF | PREV_WORKTREE_DIR] < body.txt
#   PREV_WORKTREE_DIR: diff against another worktree (for a patch that applies on top of earlier patches)
set -euo pipefail
cd "$(dirname "$0")/.."
WT=$1; NUM=$2; NAME=$3; SUBJ=$4
BASE=${5:-$(git ls-tree HEAD third_party/llama.cpp | awk '{print $3}')}
OUT=patches/$NUM-$NAME.patch
BODY=$(cat)
{
    echo "From 0000000000000000000000000000000000000000 Mon Sep 17 00:00:00 2001"
    echo "From: grammar-tax workbench <workbench@localhost>"
    echo "Date: $(date -R)"
    echo "Subject: [PATCH] $SUBJ"
    echo
    echo "$BODY"
    echo "---"
    if [ -d "$BASE" ]; then
        d=$(cd "$(dirname "$WT")" && git diff --no-index -- "$(basename "$BASE")/src" "$(basename "$WT")/src" \
            | sed -e "s#a/$(basename "$BASE")/#a/#g" -e "s#b/$(basename "$WT")/#b/#g" || true)
        echo "$d" | git apply --stat - 2>/dev/null || true
        echo
        echo "$d"
    else
        git -C "$WT" diff --stat "$BASE" -- src
        echo
        git -C "$WT" diff "$BASE" -- src
    fi
    echo "-- "
    echo "workbench"
} > "$OUT"
echo "wrote $OUT ($(grep -c '^[+-][^+-]' "$OUT") changed lines)"
