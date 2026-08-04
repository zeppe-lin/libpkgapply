#!/bin/sh
set -eu
[ "$#" -eq 2 ] || { echo 'usage: qualify-installed.sh BUILD-DIR {shared|static}' >&2; exit 2; }
build=$1; mode=$2; prefix=$build/install; deps=$(cat "$build/ci-dependency-prefix")
rm -rf "$prefix"; meson install -C "$build/product"
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$deps/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"; unset PKG_CONFIG_SYSROOT_DIR
[ "$(pkg-config --modversion libpkgapply)" = 3.0.0 ] || exit 1
req=$(pkg-config --print-requires libpkgapply)
for r in 'libpkgbuild >= 2.0.0' 'libpkgimage >= 0.4.0' 'libpkgplan >= 0.3.0' 'libpkgsource-plan >= 1.0.0'; do printf '%s\n' "$req" | grep -F "$r" >/dev/null || { echo "missing $r" >&2; exit 1; }; done
! printf '%s\n' "$req" | grep -F libpkgstate >/dev/null
flags=''; [ "$mode" = static ] && flags=--static
cxx=${CXX:-c++}
# shellcheck disable=SC2046
$cxx -std=c++17 -Wall -Wextra -Wpedantic -Werror $(pkg-config $flags --cflags libpkgapply) ci/installed-core-consumer.cpp $(pkg-config $flags --libs libpkgapply) -o "$build/installed-consumer"
LD_LIBRARY_PATH="$prefix/lib:$deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$build/installed-consumer"
for h in "$prefix"/include/libpkgapply/*.h; do unit="$build/$(basename "$h").cpp"; printf '#include <libpkgapply/%s>\n' "$(basename "$h")" >"$unit"; $cxx -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only $(pkg-config --cflags libpkgapply) "$unit"; done
if [ "$mode" = shared ]; then lib=$(find "$prefix/lib" -maxdepth 1 -type f -name 'libpkgapply.so.*' | head -n1); readelf -d "$lib" | grep -F 'Library soname: [libpkgapply.so.2]' >/dev/null; else [ -f "$prefix/lib/libpkgapply.a" ]; fi
