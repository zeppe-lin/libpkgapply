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
  man/libpkgapply-posix.3.scdoc
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

grep -F \
  'requires_private: [libpkgsource_plan_dep, libcrypto_dep]' \
  "$root/src/meson.build" >/dev/null || \
  fail 'private source projection or crypto metadata absent'

public_block=$(sed -n '/requires: \[/,/^  \],/p' "$root/src/meson.build")
if printf '%s\n' "$public_block" | grep -E \
  'libpkgsource|libcrypto|libpkgstate' >/dev/null
then
  fail 'private or foreign dependency leaked into public metadata'
fi
