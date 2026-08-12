#!/bin/sh
set -eu
root=$1
fail(){ echo "repository-contract: $*" >&2; exit 1; }
for p in include/libpkgapply src docs man tests ci .github/workflows; do [ -e "$root/$p" ] || fail "missing $p"; done
[ -s "$root/meson.options" ] || fail 'missing meson.options'
[ ! -e "$root/meson_options.txt" ] || fail 'legacy meson_options.txt remains'
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
  render-man-markdown.py check-man-markdown.py check-html-manifest.py; do
  test -x "$root/tools/$tool" || fail "missing executable tools/$tool"
done

for helper in \
  ci/qualify-html-docs.sh ci/qualify-installed-documentation.py; do
  test -x "$root/$helper" || fail "missing executable $helper"
done
