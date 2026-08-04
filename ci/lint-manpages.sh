#!/bin/sh
set -eu
[ "$#" -eq 1 ] || { echo 'usage: lint-manpages.sh BUILD-DIR' >&2; exit 2; }
find "$1/product/man" -type f \( -name 'libpkgapply.3' -o -name 'pkgapply.7' \) -exec mandoc -Tlint {} +
