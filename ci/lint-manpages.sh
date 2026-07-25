#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

[ "$#" -eq 1 ] || {
  echo "usage: lint-manpages.sh BUILD-DIR" >&2
  exit 2
}

build_dir=$1

for page in \
  man/libpkgapply.3 \
  man/libpkgapply-posix.3 \
  man/pkgapply.7
do
  test -s "$build_dir/$page" || {
    echo "generated manual is absent: $page" >&2
    exit 1
  }
  mandoc -Tlint "$build_dir/$page"
done
