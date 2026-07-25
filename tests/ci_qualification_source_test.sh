#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu
root=$1

fail()
{
  echo "ci-qualification-source-test: $*" >&2
  exit 1
}

for file in \
  "$root/ci/configure-and-test.sh" \
  "$root/ci/qualify-installed.sh" \
  "$root/ci/lint-manpages.sh" \
  "$root/ci/installed-core-consumer.cpp" \
  "$root/ci/installed-posix-consumer.cpp"
do
  test -s "$file" || fail "missing or empty ${file#"$root"/}"
done

for script in "$root"/ci/*.sh; do
  sh -n "$script" || fail "invalid shell syntax in ${script#"$root"/}"
done

for contract in \
  '--wrap-mode=nofallback' \
  'LIBPKGIMAGE_SOURCE' \
  'LIBPKGPLAN_SOURCE' \
  '-Dtests=enabled' \
  '-Dwerror=true'
do
  grep -F -- "$contract" "$root/ci/configure-and-test.sh" >/dev/null ||
    fail "configure entry point omits $contract"
done

for contract in \
  'libpkgapply-posix' \
  'libpkgapply.so.0' \
  'libpkgapply-posix.so.0' \
  'core libpkgapply metadata is contaminated by installed state' \
  'ci-dependency-prefix'
do
  grep -F "$contract" "$root/ci/qualify-installed.sh" >/dev/null ||
    fail "installed qualification omits $contract"
done

for page in libpkgapply.3 libpkgapply-posix.3 pkgapply.7; do
  grep -F "$page" "$root/ci/lint-manpages.sh" >/dev/null ||
    fail "manual qualification omits $page"
done

grep -F "version: '>=0.2.0'" "$root/meson.build" >/dev/null ||
  fail 'Meson dependency floor does not require libpkgplan 0.2.0'
grep -F "version: '>=0.3.0'" "$root/meson.build" >/dev/null ||
  fail 'Meson dependency floor does not require libpkgimage 0.3.0'
grep -F "'libpkgplan >= 0.2.0'" "$root/src/meson.build" >/dev/null ||
  fail 'core pkg-config metadata omits libpkgplan 0.2.0'
