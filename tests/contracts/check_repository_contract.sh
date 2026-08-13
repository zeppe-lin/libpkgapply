#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "repository-contract: $*" >&2; exit 1; }

for p in include/libpkgapply src docs docs/man docs/man/generated tests ci .github/workflows; do
  [ -e "$root/$p" ] || fail "missing $p"
done
[ -s "$root/meson.options" ] || fail 'missing meson.options'
[ ! -e "$root/meson_options.txt" ] || fail 'legacy meson_options.txt remains'
[ -s "$root/DESIGN.md" ] || fail 'root DESIGN.md is absent'
[ -s "$root/TESTING.md" ] || fail 'root TESTING.md is absent'
[ -s "$root/HISTORY.md" ] || fail 'root HISTORY.md is absent'
[ ! -e "$root/CHANGELOG.md" ] || fail 'retired CHANGELOG.md remains'
[ ! -e "$root/docs/architecture.md" ] || fail 'duplicate docs/architecture.md authority remains'
[ ! -e "$root/docs/testing.md" ] || fail 'duplicate docs/testing.md authority remains'
[ ! -e "$root/man" ] || fail 'legacy root man/ authority remains'

if find "$root" -type f \( -name '*.scd' -o -name '*.scdoc' \) | grep . >/dev/null; then
  fail 'scdoc manual authority remains'
fi
if git -C "$root" ls-files | grep -E \
  '(^|/)([^/]+\.(o|a|pyc)|[^/]+\.so(\..*)?)$' >/dev/null
then
  fail 'generated build product tracked'
fi

for fixture in tests/fixtures/plan.h tests/fixtures/checkpoint.h; do
  [ -s "$root/$fixture" ] || fail "missing core test fixture: $fixture"
done

find "$root/tests" -type f \( -name '*.cpp' -o -name '*.h' \) -print |
while IFS= read -r source; do
  sed -n 's/^[[:space:]]*#include[[:space:]]*"\([^"]*\)".*/\1/p' "$source" |
  while IFS= read -r header; do
    [ -f "$(dirname "$source")/$header" ] ||
      [ -f "$root/tests/$header" ] ||
      [ -f "$root/src/$header" ] ||
      fail "missing local include ${source#"$root/"}: $header"
  done
done

test -x "$root/tools/check-public-documentation.py" || fail 'public documentation checker is absent'
test -x "$root/tools/check-doxygen-contract.py" || fail 'Doxygen contract checker is absent'

grep -F -- '--include-root' "$root/tests/contracts/check_documentation_source.sh" >/dev/null ||
  fail 'documentation parser dependency binding is absent'
grep -F "pkgconfig: 'includedir'" "$root/tests/meson.build" >/dev/null ||
  fail 'Meson documentation dependency binding is absent'

for tool in \
  build-html-docs.py check-html-docs.py install-html-docs.py \
  update-man-pages.sh check-html-manifest.py; do
  test -x "$root/tools/$tool" || fail "missing executable tools/$tool"
done
[ -s "$root/tools/canonicalize-man-roff.awk" ] || fail 'roff canonicalizer is absent'
[ ! -e "$root/tools/render-man-markdown.py" ] || fail 'retired scdoc-to-Markdown renderer remains'
[ ! -e "$root/tools/check-man-markdown.py" ] || fail 'retired scdoc-to-Markdown checker remains'

for helper in ci/qualify-html-docs.sh ci/qualify-installed-documentation.py; do
  test -x "$root/$helper" || fail "missing executable $helper"
done

grep -F "input: 'generated/'" "$root/docs/man/meson.build" >/dev/null ||
  fail 'ordinary man installation is not sourced from committed generated roff'
grep -F "'update-man-pages'" "$root/docs/man/meson.build" >/dev/null ||
  fail 'manual regeneration target is absent'
grep -F "'check-man-pages'" "$root/docs/man/meson.build" >/dev/null ||
  fail 'manual freshness target is absent'

if grep -RInE 'docs/(architecture|testing)\.md|architecture\.html|test-doctrine\.html' \
    "$root/README.md" "$root/DESIGN.md" "$root/TESTING.md" "$root/docs" "$root/tools" \
    >/dev/null 2>&1; then
  fail 'retired documentation authority path remains in active source'
fi
if grep -RInF 'meson_options.txt' \
    "$root/meson.build" "$root/.github" "$root/docs" "$root/tools" \
    --exclude='check_repository_contract.sh' >/dev/null 2>&1; then
  fail 'legacy Meson options filename is still referenced'
fi

printf '%s\n' 'repository-contract: ok'
