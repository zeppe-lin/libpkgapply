#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

[ "$#" -eq 1 ] || {
  echo "usage: $0 INSTALLED-LIBRARY" >&2
  exit 2
}
library=$1
[ -s "$library" ] || {
  echo "shared-boundary-audit: missing library: $library" >&2
  exit 1
}

output=$(readelf -d "$library")
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F \
  'Library soname: [libpkgapply.so.3]' >/dev/null || {
  echo 'shared-boundary-audit: wrong SONAME' >&2
  exit 1
}
needed=$(printf '%s\n' "$output" | grep 'Shared library:' || true)
for required in \
  libpkgbuild-plan.so.1 \
  libpkgbuild-image.so.1 \
  libpkgsource-plan.so.1 \
  libpkgsource.so.3 \
  libpkgimage.so.1 \
  libpkgplan.so.1
do
  printf '%s\n' "$needed" | grep -F "[$required]" >/dev/null || {
    echo "shared-boundary-audit: missing implementation dependency: $required" >&2
    exit 1
  }
done
if printf '%s\n' "$needed" | grep -E \
  'libpkgapply-posix|libpkgstate|libpkgexec|libyaml|libarchive|libpkgresolve|libpkgcatalog' >/dev/null
then
  echo 'shared-boundary-audit: mechanism, state, resolver, or catalog dependency is present' >&2
  exit 1
fi
