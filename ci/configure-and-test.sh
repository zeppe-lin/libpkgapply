#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

usage()
{
  echo "usage: configure-and-test.sh BUILD-DIR {shared|static} [MESON-OPTION ...]" >&2
  exit 2
}

[ "$#" -ge 2 ] || usage
build_dir=$1
link_mode=$2
shift 2
case $link_mode in
  shared|static) ;;
  *) usage ;;
esac

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
source_dir=$(CDPATH= cd "$script_dir/.." && pwd)
image_source=${LIBPKGIMAGE_SOURCE:-$source_dir/subprojects/libpkgimage}
plan_source=${LIBPKGPLAN_SOURCE:-$source_dir/subprojects/libpkgplan}
source_source=${LIBPKGSOURCE_SOURCE:-$source_dir/subprojects/libpkgsource}
build_source=${LIBPKGBUILD_SOURCE:-$source_dir/subprojects/libpkgbuild}

[ -f "$image_source/meson.build" ] || {
  echo "libpkgimage source is absent: $image_source" >&2
  exit 1
}
[ -f "$plan_source/meson.build" ] || {
  echo "libpkgplan source is absent: $plan_source" >&2
  exit 1
}
[ -f "$source_source/meson.build" ] || {
  echo "libpkgsource source is absent: $source_source" >&2
  exit 1
}
[ -f "$build_source/meson.build" ] || {
  echo "libpkgbuild source is absent: $build_source" >&2
  exit 1
}

case $build_dir in
  /*) build_path=$build_dir ;;
  *) build_path=$(pwd)/$build_dir ;;
esac
dependency_prefix=$build_path/dependencies
install_prefix=$build_path/install
image_build=$build_path/libpkgimage
plan_build=$build_path/libpkgplan
source_build=$build_path/libpkgsource
build_build=$build_path/libpkgbuild

rm -rf "$dependency_prefix" "$install_prefix"
mkdir -p "$build_path"

setup()
{
  source=$1
  build=$2
  shift 2
  if [ -f "$build/meson-private/coredata.dat" ]; then
    meson setup --wipe "$build" "$source" "$@"
  else
    meson setup "$build" "$source" "$@"
  fi
}

configure_dependency()
{
  source=$1
  build=$2
  shift 2
  setup "$source" "$build" \
    --wrap-mode=nofallback \
    --fatal-meson-warnings \
    --prefix="$dependency_prefix" \
    --libdir=lib \
    -Ddefault_library="$link_mode" \
    -Dlink_mode="$link_mode" \
    -Dtests=disabled \
    -Dman_pages=disabled \
    -Dwerror=true \
    "$@"
  meson compile -C "$build"
  meson install -C "$build"
}

configure_dependency "$image_source" "$image_build"

export PKG_CONFIG_PATH=$dependency_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
unset PKG_CONFIG_SYSROOT_DIR
[ "$(pkg-config --modversion libpkgimage)" = 0.3.0 ] || {
  echo 'qualified libpkgimage is not version 0.3.0' >&2
  exit 1
}
export LD_LIBRARY_PATH=$dependency_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

configure_dependency "$plan_source" "$plan_build" -Dreference_tools=disabled
[ "$(pkg-config --modversion libpkgplan)" = 0.2.0 ] || {
  echo 'qualified libpkgplan is not version 0.2.0' >&2
  exit 1
}

configure_dependency "$source_source" "$source_build" -Dplanner_adapter=enabled
[ "$(pkg-config --modversion libpkgsource)" = 1.0.0 ] || {
  echo 'qualified libpkgsource is not version 1.0.0' >&2
  exit 1
}
[ "$(pkg-config --modversion libpkgsource-plan)" = 1.0.0 ] || {
  echo 'qualified libpkgsource-plan is not version 1.0.0' >&2
  exit 1
}

configure_dependency "$build_source" "$build_build" -Dplanner_adapter=disabled
[ "$(pkg-config --modversion libpkgbuild)" = 1.0.0 ] || {
  echo 'qualified libpkgbuild is not version 1.0.0' >&2
  exit 1
}

set -- \
  --wrap-mode=nofallback \
  --fatal-meson-warnings \
  --prefix="$install_prefix" \
  --libdir=lib \
  -Ddefault_library="$link_mode" \
  -Dlink_mode="$link_mode" \
  -Dtests=enabled \
  -Dwerror=true \
  "$@"

setup "$source_dir" "$build_dir" "$@"
meson compile -C "$build_dir"
meson test -C "$build_dir" --no-rebuild --print-errorlogs
printf '%s\n' "$dependency_prefix" >"$build_dir/ci-dependency-prefix"
printf '%s\n' "$install_prefix" >"$build_dir/ci-install-prefix"
