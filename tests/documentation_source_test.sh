#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu
root=$1

fail()
{
  echo "documentation-source-test: $*" >&2
  exit 1
}

for file in \
  "$root/CHANGELOG.md" \
  "$root/Doxyfile" \
  "$root/man/libpkgapply.3.scdoc" \
  "$root/man/libpkgapply-posix.3.scdoc" \
  "$root/man/pkgapply.7.scdoc"
do
  test -s "$file" || fail "missing or empty ${file#"$root"/}"
done

grep -F 'application_posix_backend::from_directory_fds()' \
  "$root/README.md" >/dev/null ||
  fail 'README omits the installed POSIX composition boundary'

grep -F '`application_posix_backend` factory' \
  "$root/DESIGN.md" >/dev/null ||
  fail 'design omits the implemented POSIX backend factory'

grep -F '`libpkgapply-posix` supplies the concrete caller-owned acquisition mechanism.' \
  "$root/DESIGN.md" >/dev/null ||
  fail 'design omits the concrete outer mutation lease'

grep -F '`validate_target_mutation_lease_scope()` proves that a live acquisition' \
  "$root/DESIGN.md" >/dev/null ||
  fail 'design omits target-scoped lease validation'

grep -F '*validate_target_mutation_lease_scope()* validates only' \
  "$root/man/libpkgapply.3.scdoc" >/dev/null ||
  fail 'core manual omits target-scoped lease validation'

grep -F '*target_mutation_lease::acquire()*' \
  "$root/man/libpkgapply-posix.3.scdoc" >/dev/null ||
  fail 'POSIX manual omits mutation lease acquisition'

grep -F 'Durable application-receipt encoding' \
  "$root/DESIGN.md" >/dev/null ||
  fail 'design omits durable application-receipt encoding'

grep -F '*encode_application_receipt()*' \
  "$root/man/libpkgapply.3.scdoc" >/dev/null ||
  fail 'core manual omits application-receipt codec'

grep -F 'Application-receipt codec tests prove:' \
  "$root/TESTING.md" >/dev/null ||
  fail 'testing contract omits application-receipt codec'

grep -F 'incoming rejected staging' "$root/TESTING.md" >/dev/null ||
  fail 'testing contract omits concrete rejected-route qualification'

grep -F 'incoming_package_authority::admit()' "$root/DESIGN.md" >/dev/null ||
  fail 'design omits native incoming build admission'

grep -F 'Complete or recover every durable 0.1.0 attempt'   "$root/CHANGELOG.md" >/dev/null ||
  fail 'release record omits durable-attempt transition'

for stale in \
  'The remaining POSIX boundary' \
  'The complete backend will compose' \
  'which will rebuild the private session' \
  'Version 0.1.0 is being built contract-first'
do
  if grep -F "$stale" "$root/README.md" "$root/DESIGN.md" \
      "$root/TESTING.md" >/dev/null; then
    fail "documentation retains stale implementation claim: $stale"
  fi
done

grep -F 'PROJECT_NUMBER         = 2.3.0' "$root/Doxyfile" >/dev/null ||
  fail 'Doxygen project version is not 2.3.0'
