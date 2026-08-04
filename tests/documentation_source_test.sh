#!/bin/sh
set -eu
root=$1
fail(){ echo "documentation-source-test: $*" >&2; exit 1; }
for f in README.md DESIGN.md TESTING.md CHANGELOG.md Doxyfile man/libpkgapply.3.scdoc man/pkgapply.7.scdoc docs/architecture.md docs/integration.md docs/abi.md docs/testing.md docs/history/3.0-posix-extraction.md; do [ -s "$root/$f" ] || fail "missing $f"; done
[ ! -e "$root/man/libpkgapply-posix.3.scdoc" ] || fail 'POSIX manual remains in core'
grep -F 'does not depend outward on it' "$root/docs/architecture.md" >/dev/null || fail 'dependency direction absent'
grep -F 'Do not tag 3.0' "$root/MAINTAINING.md" >/dev/null || fail 'ABI release gate absent'
