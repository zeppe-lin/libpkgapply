#!/bin/sh
set -eu

root=$1
fail() {
  echo "architecture-contract: $*" >&2
  exit 1
}

for path in \
  posix \
  include/libpkgapply-posix \
  docs/man/libpkgapply-posix.3.md
do
  [ ! -e "$root/$path" ] || fail "extracted product remains: $path"
done

if grep -R -E \
  '^#include <(sys/|unistd.h|fcntl.h|dirent.h|syscall.h)' \
  "$root/include/libpkgapply" "$root/src" >/dev/null
then
  fail 'POSIX system dependency in core'
fi

if grep -R -E 'libpkgstate|pkgstate::' \
  "$root/include" "$root/src" "$root/meson.build" >/dev/null
then
  fail 'state publication contamination'
fi

if grep -F 'fallback:' "$root/meson.build" >/dev/null; then
  fail 'fallback coupling remains'
fi

for forbidden in libpkgbuild libpkgbuild-image libpkgsource libpkgsource-plan libpkgimage; do
  if grep -F "dependency('$forbidden'" "$root/meson.build" >/dev/null; then
    fail "foreign upstream dependency leaked into core root: $forbidden"
  fi
done

grep -F "dependency(" "$root/meson.build" >/dev/null || fail 'dependency declarations absent'
grep -F "'libpkgbuild-plan'," "$root/meson.build" >/dev/null || \
  fail 'planner-ready build authority dependency absent'
grep -F "'libpkgplan'," "$root/meson.build" >/dev/null || \
  fail 'accepted-plan dependency absent'

grep -F 'requires_private: [libcrypto_dep]' "$root/src/meson.build" >/dev/null || \
  fail 'private crypto metadata absent'

public_block=$(sed -n '/requires: \[/,/^  \],/p' "$root/src/meson.build")
for required in 'libpkgbuild-plan >= 1.1.0' 'libpkgbuild-plan < 2.0.0' \
                'libpkgplan >= 0.3.0' 'libpkgplan < 1.0.0'; do
  printf '%s\n' "$public_block" | grep -F "$required" >/dev/null || \
    fail "missing public requirement: $required"
done
if printf '%s\n' "$public_block" | grep -E \
  'libpkgsource|libpkgbuild([^-]|$)|libpkgbuild-image|libpkgimage|libcrypto|libpkgstate' >/dev/null
then
  fail 'private, transitive, or foreign dependency leaked into public metadata'
fi
