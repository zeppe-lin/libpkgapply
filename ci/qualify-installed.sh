#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

usage()
{
  echo "usage: qualify-installed.sh BUILD-DIR {shared|static}" >&2
  exit 2
}

[ "$#" -eq 2 ] || usage
build_dir=$1
link_mode=$2

case $link_mode in
  shared|static) ;;
  *) usage ;;
esac

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)

for marker in ci-dependency-prefix ci-install-prefix; do
  [ -f "$build_dir/$marker" ] || {
    echo "build directory has no $marker" >&2
    exit 1
  }
done
dependency_prefix=$(cat "$build_dir/ci-dependency-prefix")
prefix=$(cat "$build_dir/ci-install-prefix")
case $build_dir in
  /*) build_path=$build_dir ;;
  *) build_path=$(pwd)/$build_dir ;;
esac
expected_dependency_prefix=$build_path/dependencies
expected_prefix=$build_path/install
[ "$dependency_prefix" = "$expected_dependency_prefix" ] || {
  echo "refusing unexpected dependency prefix '$dependency_prefix'" >&2
  exit 1
}
[ "$prefix" = "$expected_prefix" ] || {
  echo "refusing unexpected installation prefix '$prefix'" >&2
  exit 1
}

rm -rf "$prefix"
meson install -C "$build_dir"

tmp=$(mktemp -d "${TMPDIR:-/tmp}/libpkgapply-consumer.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

export PKG_CONFIG_PATH=$prefix/lib/pkgconfig:$dependency_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
unset PKG_CONFIG_SYSROOT_DIR

for package in libpkgapply libpkgapply-posix; do
  version=$(pkg-config --modversion "$package")
  [ "$version" = 1.0.0 ] || {
    echo "installed $package version is '$version', expected '1.0.0'" >&2
    exit 1
  }
done

if pkg-config --print-requires libpkgapply | grep -F libpkgstate >/dev/null ||
   pkg-config --print-requires-private libpkgapply | grep -F libpkgstate >/dev/null
then
  echo "core libpkgapply metadata is contaminated by installed state" >&2
  exit 1
fi

cxx=${CXX:-c++}
core_consumer=$tmp/installed-core-consumer
posix_consumer=$tmp/installed-posix-consumer

case $link_mode in
  shared)
    # shellcheck disable=SC2046
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
      $(pkg-config --cflags libpkgapply) \
      "$script_dir/installed-core-consumer.cpp" \
      $(pkg-config --libs libpkgapply) \
      -o "$core_consumer"
    # shellcheck disable=SC2046
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
      $(pkg-config --cflags libpkgapply-posix) \
      "$script_dir/installed-posix-consumer.cpp" \
      $(pkg-config --libs libpkgapply-posix) \
      -o "$posix_consumer"
    ;;
  static)
    # shellcheck disable=SC2046
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
      $(pkg-config --static --cflags libpkgapply) \
      "$script_dir/installed-core-consumer.cpp" \
      $(pkg-config --static --libs libpkgapply) \
      -o "$core_consumer"
    # shellcheck disable=SC2046
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
      $(pkg-config --static --cflags libpkgapply-posix) \
      "$script_dir/installed-posix-consumer.cpp" \
      $(pkg-config --static --libs libpkgapply-posix) \
      -o "$posix_consumer"
    ;;
esac

for directory in libpkgapply libpkgapply-posix; do
  package=$directory
  for header in "$prefix/include/$directory"/*.h; do
    unit=$tmp/$(basename "$directory")-$(basename "$header").cpp
    printf '#include <%s/%s>\n' "$directory" "$(basename "$header")" >"$unit"
    # shellcheck disable=SC2046
    "$cxx" -std=c++17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only \
      $(pkg-config --cflags "$package") "$unit"
  done
done

runtime_path=$prefix/lib:$dependency_prefix/lib
LD_LIBRARY_PATH=$runtime_path${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} \
  "$core_consumer"
LD_LIBRARY_PATH=$runtime_path${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} \
  "$posix_consumer"

case $link_mode in
  shared)
    core_library=$(find "$prefix/lib" -maxdepth 1 -type f \
      -name 'libpkgapply.so.*' -print | sort | head -n 1)
    posix_library=$(find "$prefix/lib" -maxdepth 1 -type f \
      -name 'libpkgapply-posix.so.*' -print | sort | head -n 1)
    [ -n "$core_library" ] && [ -n "$posix_library" ] || {
      echo "installed shared application libraries are absent" >&2
      exit 1
    }
    readelf -d "$core_library" | grep -q 'SONAME.*libpkgapply\.so\.1' || {
      echo "core shared library SONAME is not libpkgapply.so.1" >&2
      exit 1
    }
    readelf -d "$posix_library" | grep -q 'SONAME.*libpkgapply-posix\.so\.1' || {
      echo "POSIX shared library SONAME is not libpkgapply-posix.so.1" >&2
      exit 1
    }
    readelf -d "$core_library" | grep -q 'libpkgbuild\.so\.' || {
      echo "shared libpkgapply does not record libpkgbuild" >&2
      exit 1
    }
    readelf -d "$core_library" | grep -q 'libpkgsource-plan\.so\.' || {
      echo "shared libpkgapply does not record libpkgsource-plan" >&2
      exit 1
    }
    readelf -d "$core_library" | grep -q 'libpkgplan\.so\.' || {
      echo "shared libpkgapply does not record libpkgplan" >&2
      exit 1
    }
    readelf -d "$core_library" | grep -q 'libpkgimage\.so\.' || {
      echo "shared libpkgapply does not record libpkgimage" >&2
      exit 1
    }
    readelf -d "$core_library" | grep -q 'libcrypto\.so\.' || {
      echo "shared libpkgapply does not record libcrypto" >&2
      exit 1
    }
    readelf -d "$posix_library" | grep -q 'libpkgapply\.so\.' || {
      echo "shared libpkgapply-posix does not record core libpkgapply" >&2
      exit 1
    }
    ;;
  static)
    [ -f "$prefix/lib/libpkgapply.a" ] || {
      echo "installed static libpkgapply archive is absent" >&2
      exit 1
    }
    [ -f "$prefix/lib/libpkgapply-posix.a" ] || {
      echo "installed static libpkgapply-posix archive is absent" >&2
      exit 1
    }
    pkg-config --static --libs libpkgapply >/dev/null
    pkg-config --static --libs libpkgapply-posix >/dev/null
    ;;
esac
