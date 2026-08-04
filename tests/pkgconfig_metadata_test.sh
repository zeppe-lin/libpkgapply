#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
build=$1
pc=$build/meson-private/libpkgapply.pc
[ -s "$pc" ] || { echo "pkgconfig-metadata: missing $pc" >&2; exit 1; }
grep -F 'Version: 3.0.0' "$pc" >/dev/null
grep -F -- '-lpkgapply' "$pc" >/dev/null
public=$(sed -n 's/^Requires:[[:space:]]*//p' "$pc")
private=$(sed -n 's/^Requires\.private:[[:space:]]*//p' "$pc")
private_libs=$(sed -n 's/^Libs\.private:[[:space:]]*//p' "$pc")
has_requirement() {
  printf '%s\n' "$1" | tr ',' '\n' | awk \
    -v package="$2" -v version="$3" '
      $1 == package && $2 == ">=" && $3 == version { found = 1 }
      END { exit found ? 0 : 1 }
    '
}
for requirement in \
  'libpkgbuild 2.0.0' \
  'libpkgimage 0.4.0' \
  'libpkgplan 0.3.0'
do
  set -- $requirement
  has_requirement "$public" "$1" "$2" || {
    echo "pkgconfig-metadata: missing public $1 >= $2" >&2
    exit 1
  }
done
if printf '%s\n' "$public" | grep -E 'libpkgsource-plan|libcrypto|libpkgstate' >/dev/null; then
  echo 'pkgconfig-metadata: private or foreign edge leaked publicly' >&2
  exit 1
fi
has_requirement "$private" libpkgsource-plan 1.0.0 || {
  echo 'pkgconfig-metadata: missing private libpkgsource-plan >= 1.0.0' >&2
  exit 1
}
printf '%s\n%s\n' "$private" "$private_libs" | grep -F libcrypto >/dev/null || {
  echo 'pkgconfig-metadata: missing private libcrypto' >&2
  exit 1
}
