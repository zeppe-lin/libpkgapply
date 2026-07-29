#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu
root=$1

fail()
{
  echo "release-metadata-test: $*" >&2
  exit 1
}

grep -F "version: '2.0.0'" "$root/meson.build" >/dev/null ||
  fail 'Meson project version is not 1.0.0'
grep -F 'PROJECT_NUMBER         = 2.0.0' "$root/Doxyfile" >/dev/null ||
  fail 'Doxygen project version is not 1.0.0'
grep -F 'return "2.0.0";' "$root/src/version.cpp" >/dev/null ||
  fail 'runtime version is not 1.0.0'
grep -F 'inline constexpr std::uint32_t api_version = 2;'   "$root/include/libpkgapply/version.h" >/dev/null ||
  fail 'public API version is not 1'

test "$(grep -Fc "soversion: '2'" "$root/src/meson.build")" -eq 1 ||
  fail 'core SOVERSION is not exactly 1'
test "$(grep -Fc "soversion: '2'" "$root/posix/meson.build")" -eq 1 ||
  fail 'POSIX SOVERSION is not exactly 1'

first_release=$(
  sed -n '/^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]* - / {
    s/ - .*//
    p
    q
  }' "$root/CHANGELOG.md"
)
[ "$first_release" = 2.0.0 ] ||
  fail "CHANGELOG first release is '$first_release', expected '2.0.0'"

grep -F 'Version 2.0.0 does not publish installed state'   "$root/CHANGELOG.md" >/dev/null ||
  fail 'release record omits installed-state boundary'
grep -F 'libpkgbuild 2.0.0' "$root/CHANGELOG.md" >/dev/null ||
  fail 'release record omits native build authority floor'
grep -F 'libpkgsource-plan 2.0.0' "$root/CHANGELOG.md" >/dev/null ||
  fail 'release record omits source projection floor'
