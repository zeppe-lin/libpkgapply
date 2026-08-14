#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

usage() {
  echo 'usage: qualify-installed.sh BUILD-DIR {shared|static}' >&2
  exit 2
}

[ "$#" -eq 2 ] || usage
build=$1
mode=$2
case $mode in
  shared|static) ;;
  *) usage ;;
esac

prefix=$build/install
deps=$(cat "$build/ci-dependency-prefix")
rm -rf "$prefix"
meson install -C "$build/product"

export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$deps/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$prefix/lib:$deps/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
unset PKG_CONFIG_SYSROOT_DIR

[ "$(pkg-config --modversion libpkgapply)" = 3.0.1 ] || {
  echo 'installed libpkgapply version is not 3.0.1' >&2
  exit 1
}

public=$(pkg-config --print-requires libpkgapply)
for requirement in \
  'libpkgbuild-plan >= 1.1.0' \
  'libpkgbuild-plan < 2.0.0' \
  'libpkgplan >= 0.3.0' \
  'libpkgplan < 1.0.0'
do
  printf '%s\n' "$public" | grep -F "$requirement" >/dev/null || {
    echo "missing public requirement: $requirement" >&2
    exit 1
  }
done
if printf '%s\n' "$public" | grep -E \
  'libpkgsource|libpkgbuild([^-]|$)|libpkgbuild-image|libpkgimage|libcrypto|libpkgstate' >/dev/null
then
  echo 'private, transitive, or foreign dependency leaked into Requires' >&2
  exit 1
fi

private=$(pkg-config --print-requires-private libpkgapply)
if printf '%s\n' "$private" | grep -E \
  'libpkgsource|libpkgbuild|libpkgimage|libpkgplan|libpkgstate' >/dev/null
then
  echo 'semantic authority leaked into private metadata' >&2
  exit 1
fi
printf '%s\n' "$private" | grep -F libcrypto >/dev/null || {
  echo 'missing private libcrypto requirement' >&2
  exit 1
}

flags=
[ "$mode" = static ] && flags=--static
cxx=${CXX:-c++}

# shellcheck disable=SC2046
$cxx -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  $(pkg-config $flags --cflags libpkgapply) \
  ci/installed-core-consumer.cpp \
  $(pkg-config $flags --libs libpkgapply) \
  -o "$build/installed-consumer"
"$build/installed-consumer"

for header in "$prefix"/include/libpkgapply/*.h; do
  unit=$build/$(basename "$header").cpp
  printf '#include <libpkgapply/%s>\n' "$(basename "$header")" >"$unit"
  # shellcheck disable=SC2046
  $cxx -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
    $(pkg-config --cflags libpkgapply) "$unit"
done

if [ "$mode" = shared ]; then
  library=$(find "$prefix/lib" -maxdepth 1 -type f \
    -name 'libpkgapply.so.*' -print -quit)
  [ -n "$library" ] || {
    echo 'installed shared library not found' >&2
    exit 1
  }
  "$(dirname "$0")/audit-shared-boundary.sh" "$library"
else
  [ -f "$prefix/lib/libpkgapply.a" ] || {
    echo 'installed static archive not found' >&2
    exit 1
  }
fi

python3 ci/qualify-installed-documentation.py "$prefix" libpkgapply

for page in "$build"/product/docs/man/*.[137]; do
  [ -e "$page" ] || continue
  section=${page##*.}
  installed=$prefix/share/man/man$section/$(basename "$page")
  [ -s "$installed" ] || {
    echo "installed manual is absent: $installed" >&2
    exit 1
  }
done
