#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu
root=$1

fail()
{
  echo "journal-projection-evidence-source-test: $*" >&2
  exit 1
}

header=$root/include/libpkgapply/journal.h
source=$root/src/journal.cpp
codec=$root/src/journal_codec.cpp
test_source=$root/tests/protocol/journal_codec_test.cpp

for file in "$header" "$source" "$codec" "$test_source"; do
  test -s "$file" || fail "missing or empty ${file#"$root"/}"
done

for contract in \
  'lease_bound_state_projection state_projection' \
  'admitted_state_projection() const noexcept' \
  'lease_bound_state_projection state_projection_'
do
  grep -F -- "$contract" "$header" >/dev/null ||
    fail "journal header omits exact projection-body contract: $contract"
done

for contract in \
  'state_projection.lease() != lease' \
  'state_projection_.identity()' \
  'admitted_state_projection() const noexcept'
do
  grep -F -- "$contract" "$source" >/dev/null ||
    fail "journal implementation omits projection-body invariant: $contract"
done

for contract in \
  'header.admitted_state_projection()' \
  'projection.paths()' \
  'lease_bound_state_projection::make('
do
  grep -F -- "$contract" "$codec" >/dev/null ||
    fail "journal codec omits durable projection body: $contract"
done

grep -F -- 'admitted_state_projection().identity()' "$test_source" >/dev/null ||
  fail 'protocol test does not verify retained projection identity'
grep -F -- 'admitted_state_projection().paths()' "$test_source" >/dev/null ||
  fail 'protocol test does not verify retained projection path body'
grep -F -- 'pkgplan::package_path::parse("usr/bin/tool")' "$test_source" >/dev/null ||
  fail 'protocol projection fixture does not carry a concrete path body'
grep -F -- 'installed_package_identity' "$test_source" >/dev/null ||
  fail 'protocol projection fixture does not carry installed-owner evidence'

if grep -F -- 'lease_bound_state_projection_identity state_projection' \
    "$header" >/dev/null; then
  fail 'journal admission regressed to naked projection identity'
fi
