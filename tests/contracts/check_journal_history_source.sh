#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:-.}
fail(){ echo "journal-history-source: $*" >&2; exit 1; }
header=$root/src/journal_history.h
source=$root/src/journal_history.cpp
test_source=$root/tests/integration/journal_history_test.cpp
[ -s "$header" ] || fail 'owner journal-history header is absent'
[ -s "$source" ] || fail 'owner journal-history source is absent'
[ -s "$test_source" ] ||
  fail 'owner journal-history integration witness is absent'

grep -F 'std::unordered_map<application_journal_effect_identity,' \
  "$header" >/dev/null ||
  fail 'effect progress lacks identity index'
grep -F 'sequence < committed.step_count();' "$source" >/dev/null ||
  fail 'committed history is not loaded by exact sequence'
grep -F 'store.load_step(declaration_identity, committed.step_count())' \
  "$source" >/dev/null ||
  fail 'exact next-step orphan probe is absent'
grep -F 'committed.identity(), history.cursor()' "$source" >/dev/null ||
  fail 'orphan adoption is not cursor-CAS bound'

for forbidden in \
  'directory_iterator' 'readdir(' 'scandir(' '.observe(' 'publish_journal('; do
  if grep -F "$forbidden" "$source" >/dev/null; then
    fail "owner rehydration contains forbidden mechanism: $forbidden"
  fi
done

grep -F 'constexpr std::size_t effect_count = 10000;' \
  "$test_source" >/dev/null ||
  fail '10,000-effect retained-history assault is absent'
grep -F 'history.cursor().step_count() == 20000' "$test_source" >/dev/null ||
  fail '20,000-step retained-history witness is absent'
grep -F 'store.step_loads() - loads_before == 20001' \
  "$test_source" >/dev/null ||
  fail 'exact-read scaling assertion is absent'
