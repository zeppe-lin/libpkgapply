#!/bin/sh
set -eu
root=$1
fail(){ echo "architecture-contract: $*" >&2; exit 1; }
for p in posix include/libpkgapply-posix man/libpkgapply-posix.3.scdoc; do [ ! -e "$root/$p" ] || fail "extracted product remains: $p"; done
! grep -R -E '^#include <(sys/|unistd.h|fcntl.h|dirent.h|syscall.h)' "$root/include/libpkgapply" "$root/src" >/dev/null || fail 'POSIX system dependency in core'
! grep -R -E 'libpkgstate|pkgstate::' "$root/include" "$root/src" "$root/meson.build" >/dev/null || fail 'state publication contamination'
! grep -F 'fallback:' "$root/meson.build" >/dev/null || fail 'fallback coupling remains'
