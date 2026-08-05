#!/bin/sh
set -eu
root=$1
build_root=$2
fail(){ echo "style-contract: $*" >&2; exit 1; }
for f in .clang-format .editorconfig docs/code-style.md; do [ -s "$root/$f" ] || fail "missing $f"; done
if find "$root" -path "$root/.git" -prune -o -path "$build_root" -prune -o -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.md' -o -name '*.build' -o -name '*.txt' -o -name '*.sh' -o -name '*.yml' \) -exec grep -Il '' {} + | xargs -r grep -n "$(printf '\t')" >/dev/null; then fail 'tab character present'; fi
