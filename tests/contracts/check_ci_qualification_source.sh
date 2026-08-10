#!/bin/sh
set -eu
root=$1
fail(){ echo "ci-qualification-source-test: $*" >&2; exit 1; }
for f in .github/workflows/ci.yml ci/configure-and-test.sh ci/qualify-installed.sh ci/lint-manpages.sh ci/audit-shared-boundary.sh ci/installed-core-consumer.cpp; do [ -s "$root/$f" ] || fail "missing $f"; done
[ ! -e "$root/ci/installed-posix-consumer.cpp" ] || fail 'POSIX installed consumer remains'
find "$root/ci" "$root/tests" -type f -name '*.sh' -exec sh -n {} \; ||\
  fail 'invalid shell script in ci/ or tests/'
for token in v0.4.0 v0.3.1 v3.0.1 v3.1.0 v1.1.0 v1.0.0 v2.0.0; do grep -F "$token" "$root/.github/workflows/ci.yml" >/dev/null || fail "CI omits $token"; done

for token in 'GCC shared' 'GCC static' 'Clang shared' 'Clang static' 'GCC release' 'address,undefined' 'meson==1.10.2'; do
  grep -F "$token" "$root/.github/workflows/ci.yml" >/dev/null || fail "CI omits $token"
done

grep -F 'html_docs: enabled' "$root/.github/workflows/ci.yml" >/dev/null || fail 'GCC shared HTML build is absent'
grep -F 'pandoc' "$root/.github/workflows/ci.yml" >/dev/null || fail 'Pandoc qualification dependency is absent'
grep -F -- '-Dhtml_docs=' "$root/.github/workflows/ci.yml" >/dev/null || fail 'HTML Meson feature is not configured'
grep -F 'qualify-html-docs.sh' "$root/.github/workflows/ci.yml" >/dev/null || fail 'installed HTML qualification is absent'

grep -F 'application_target_context::make' "$root/ci/installed-core-consumer.cpp" >/dev/null || fail 'installed consumer does not execute core semantics'
for token in 'canonical_domain()' '.algorithm()' '.string()' '.bytes()'; do
  grep -F "$token" "$root/ci/installed-core-consumer.cpp" >/dev/null ||
    fail "installed consumer does not exercise public typed-digest ABI: $token"
done
grep -F 'projection_error dependency_probe' "$root/ci/installed-core-consumer.cpp" >/dev/null || fail 'installed consumer does not force public libpkgbuild-plan closure'
if grep -F 'auto* volatile' "$root/ci/installed-core-consumer.cpp" >/dev/null; then fail 'installed consumer regressed to address-only linkage'; fi
