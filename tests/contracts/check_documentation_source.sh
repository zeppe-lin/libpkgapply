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

for f in \
  README.md DESIGN.md TESTING.md HISTORY.md CONTRIBUTING.md MAINTAINING.md Doxyfile \
  docs/integration.md docs/abi.md docs/code-style.md docs/html.md \
  docs/manpage-markdown.md docs/history/3.0-posix-extraction.md \
  docs/man/libpkgapply.3.md docs/man/pkgapply.7.md
do
  [ -s "$root/$f" ] || fail "missing $f"
done
for f in \
  README.md DESIGN.md TESTING.md HISTORY.md CONTRIBUTING.md MAINTAINING.md \
  docs/integration.md docs/abi.md docs/code-style.md docs/html.md \
  docs/manpage-markdown.md docs/history/3.0-posix-extraction.md
do
  case $(sed -n '1p' "$root/$f") in
    '# '*) ;;
    *) fail "$f does not start with an ATX level-one heading" ;;
  esac
  count=$(grep -c '^# ' "$root/$f" || true)
  [ "$count" -eq 1 ] || fail "$f must contain exactly one ATX level-one heading"
done

if grep -RInE '^[-=~]{3,}[[:space:]]*$' \
    "$root"/*.md "$root"/docs/*.md "$root"/docs/history/*.md >/dev/null 2>&1; then
  fail 'underline-style Markdown heading remains'
fi
[ ! -e "$root/CHANGELOG.md" ] || fail 'retired CHANGELOG.md authority remains'
[ ! -e "$root/docs/architecture.md" ] || fail 'duplicate docs/architecture.md authority remains'
[ ! -e "$root/docs/testing.md" ] || fail 'duplicate docs/testing.md authority remains'
[ ! -e "$root/man" ] || fail 'legacy root man/ authority remains'
if find "$root" -type f \( -name '*.scd' -o -name '*.scdoc' \) | grep . >/dev/null; then
  fail 'scdoc manual authority remains'
fi

grep -F 'the core never depends outward on its reference mechanism provider' \
  "$root/DESIGN.md" >/dev/null || fail 'dependency direction absent'
grep -F 'never to a fresh provider callback' "$root/DESIGN.md" >/dev/null ||
  fail 'request-bound backend authority absent'
grep -F 'complete dynamic symbol table with the reviewed generation-4 ELF manifest' \
  "$root/TESTING.md" >/dev/null || fail 'ABI symbol qualification absent'
grep -F 'The current ABI gate is closed only while' "$root/MAINTAINING.md" >/dev/null ||
  fail 'closed ABI release gate absent'

if grep -F 'The lifecycle-executor identity is explicitly absent in schema version 1.' \
    "$root/DESIGN.md" >/dev/null; then
  fail 'stale lifecycle-executor schema claim remains'
fi
if grep -F 'Lifecycle exclusion in version 0.1' "$root/DESIGN.md" >/dev/null; then
  fail 'obsolete lifecycle-exclusion section remains'
fi
grep -F 'Restart under a newly acquired' "$root/DESIGN.md" >/dev/null ||
  fail 'restart completed-evidence projection refresh is undocumented'
grep -F 'Completed-evidence publication is immutable and idempotent.' \
    "$root/docs/man/libpkgapply.3.md" >/dev/null ||
  fail 'public manual omits completed-evidence restart refresh'

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

python3 "$root/tools/check-html-manifest.py" \
  --root "$root" --project libpkgapply
