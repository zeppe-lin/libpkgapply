#!/bin/sh
set -eu
root=$1
fail(){ echo "release-metadata-test: $*" >&2; exit 1; }
grep -F "version: '4.0.0'" "$root/meson.build" >/dev/null || fail 'Meson version mismatch'
grep -F 'PROJECT_NUMBER         = 4.0.0' "$root/Doxyfile" >/dev/null || fail 'Doxygen version mismatch'
grep -F 'return "4.0.0";' "$root/src/version.cpp" >/dev/null || fail 'runtime version mismatch'
grep -F 'api_version = 4' "$root/include/libpkgapply/version.h" >/dev/null || fail 'API generation mismatch'
grep -F "soversion: '4'" "$root/src/meson.build" >/dev/null || fail 'SONAME mismatch'
[ ! -d "$root/posix" ] || fail 'POSIX source remains'
grep -F '## Unreleased' "$root/HISTORY.md" >/dev/null || fail 'development history section absent'
grep -F 'Generation-4 append-only application history' "$root/HISTORY.md" >/dev/null || fail 'generation-4 history absent'
grep -F 'Advanced the core to SONAME 4 and public API generation 4' "$root/HISTORY.md" >/dev/null || fail 'generation-4 ABI transition undocumented'
