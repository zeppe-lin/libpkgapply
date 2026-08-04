#!/bin/sh
set -eu
root=$1
fail(){ echo "repository-contract: $*" >&2; exit 1; }
for p in include/libpkgapply src docs man tests ci .github/workflows; do [ -e "$root/$p" ] || fail "missing $p"; done
! find "$root" -path "$root/.git" -prune -o -type f \( -name '*.o' -o -name '*.a' -o -name '*.so' -o -name '*.pyc' \) -print | grep . >/dev/null || fail 'build product present'

test -x "$root/tools/check-public-documentation.py" || fail 'public documentation checker is absent'
