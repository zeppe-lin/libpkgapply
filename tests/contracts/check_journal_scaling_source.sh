#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "journal-scaling-source: $*" >&2; exit 1; }
source=$root/src/journal.cpp
test_source=$root/tests/unit/journal_test.cpp
block=$(sed -n '/^validate_events(/,/^bool$/p' "$source")
printf '%s\n' "$block" | grep -F 'effect_ordinals.reserve(effects.size());' >/dev/null ||
  fail 'journal validation lacks one effect-identity index'
printf '%s\n' "$block" | grep -F 'effect_ordinals.find(event.effect())' >/dev/null ||
  fail 'journal event lookup does not use the effect-identity index'
if printf '%s\n' "$block" | grep -F 'std::find_if' >/dev/null; then
  fail 'journal validation rescans the effect graph per event'
fi
grep -F 'constexpr std::size_t effect_count = 4096;' "$test_source" >/dev/null ||
  fail 'large single-snapshot journal witness is absent'
grep -F 'std::move(large_effects), std::move(large_events)' "$test_source" >/dev/null ||
  fail 'large journal witness does not validate one complete retained snapshot'
