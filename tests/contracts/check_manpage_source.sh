#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "manpage-source-test: $*" >&2; exit 1; }

for page in libpkgapply.3 pkgapply.7
do
  source=$root/docs/man/$page.md
  [ -s "$source" ] || fail "missing canonical source: docs/man/$page.md"
  first=$(sed -n '1p' "$source")
  case $first in
    "% "*"("[1-9]") libpkgapply | Version 3.0.1") ;;
    *) fail "invalid Pandoc title in docs/man/$page.md: $first" ;;
  esac
  grep -F '# NAME' "$source" >/dev/null || fail "NAME section missing: $page"
  grep -F '# SEE ALSO' "$source" >/dev/null || fail "SEE ALSO section missing: $page"
done

if find "$root/docs/man" -maxdepth 1 -type f \( -name '*.scd' -o -name '*.scdoc' \) | grep . >/dev/null; then
  fail 'scdoc source remains'
fi
if grep -RInE '^[-=]{3,}$' "$root/docs/man" --include='*.md' >/dev/null; then
  fail 'Setext heading remains in manual source'
fi
