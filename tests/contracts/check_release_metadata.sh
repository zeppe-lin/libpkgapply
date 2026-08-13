#!/bin/sh
set -eu
root=$1
fail(){ echo "release-metadata-test: $*" >&2; exit 1; }
grep -F "version: '3.0.0'" "$root/meson.build" >/dev/null || fail 'Meson version mismatch'
grep -F 'PROJECT_NUMBER         = 3.0.0' "$root/Doxyfile" >/dev/null || fail 'Doxygen version mismatch'
grep -F 'return "3.0.0";' "$root/src/version.cpp" >/dev/null || fail 'runtime version mismatch'
grep -F 'api_version = 3' "$root/include/libpkgapply/version.h" >/dev/null || fail 'API generation mismatch'
grep -F "soversion: '3'" "$root/src/meson.build" >/dev/null || fail 'SONAME mismatch'
[ ! -d "$root/posix" ] || fail 'POSIX source remains'
grep -F '## 3.0.0 - 2026-08-12' "$root/HISTORY.md" >/dev/null || fail '3.0 release history absent'
! grep -F '3.0.0 - unreleased' "$root/HISTORY.md" >/dev/null || fail '3.0 release remains marked unreleased'
