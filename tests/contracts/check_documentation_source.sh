#!/bin/sh
set -eu
root=$1
shift
fail(){ echo "documentation-source-test: $*" >&2; exit 1; }

resolve_include()
{
  package=$1
  value=$2
  if [ -n "$value" ]; then
    printf '%s\n' "$value"
    return
  fi
  command -v pkg-config >/dev/null 2>&1 ||
    fail "$package include root is unavailable"
  pkg-config --exists "$package" ||
    fail "$package include root is unavailable"
  pkg-config --variable=includedir "$package"
}

next_include()
{
  package=$1
  value=${2:-}
  resolve_include "$package" "$value"
}

build_plan_include=$(next_include libpkgbuild-plan "${1:-}"); [ "$#" -eq 0 ] || shift
plan_include=$(next_include libpkgplan "${1:-}"); [ "$#" -eq 0 ] || shift
build_image_include=$(next_include libpkgbuild-image "${1:-}"); [ "$#" -eq 0 ] || shift
build_include=$(next_include libpkgbuild "${1:-}"); [ "$#" -eq 0 ] || shift
image_include=$(next_include libpkgimage "${1:-}"); [ "$#" -eq 0 ] || shift
source_plan_include=$(next_include libpkgsource-plan "${1:-}"); [ "$#" -eq 0 ] || shift
source_include=$(next_include libpkgsource "${1:-}"); [ "$#" -eq 0 ] || shift
resolve_include_root=$(next_include libpkgresolve "${1:-}"); [ "$#" -eq 0 ] || shift
catalog_include=$(next_include libpkgcatalog "${1:-}"); [ "$#" -eq 0 ] || shift
state_include=$(next_include libpkgstate "${1:-}"); [ "$#" -eq 0 ] || shift
[ "$#" -eq 0 ] || fail 'unexpected documentation-contract argument'

for f in README.md DESIGN.md TESTING.md CHANGELOG.md Doxyfile man/libpkgapply.3.scdoc man/pkgapply.7.scdoc docs/architecture.md docs/integration.md docs/abi.md docs/testing.md docs/history/3.0-posix-extraction.md; do [ -s "$root/$f" ] || fail "missing $f"; done
[ ! -e "$root/man/libpkgapply-posix.3.scdoc" ] || fail 'POSIX manual remains in core'
grep -F 'does not depend outward on it' "$root/docs/architecture.md" >/dev/null || fail 'dependency direction absent'
grep -F 'Do not tag 3.0' "$root/MAINTAINING.md" >/dev/null || fail 'ABI release gate absent'
python3 "$root/tools/check-public-documentation.py" \
  "$root" libpkgapply libpkgapply.h
if command -v clang++ >/dev/null 2>&1; then
  python3 "$root/tools/check-doxygen-contract.py" \
    --root "$root" --include-subdir libpkgapply \
    --include-root "$build_plan_include" \
    --include-root "$plan_include" \
    --include-root "$build_image_include" \
    --include-root "$build_include" \
    --include-root "$image_include" \
    --include-root "$source_plan_include" \
    --include-root "$source_include" \
    --include-root "$resolve_include_root" \
    --include-root "$catalog_include" \
    --include-root "$state_include" \
    --namespace pkgapply --clang "$(command -v clang++)"
fi

python3 "$root/tools/check-man-markdown.py" \
  --root "$root" --project libpkgapply --version 3.0.0
python3 "$root/tools/check-html-manifest.py" \
  --root "$root" --project libpkgapply
