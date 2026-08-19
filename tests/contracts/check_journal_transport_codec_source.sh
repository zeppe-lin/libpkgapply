#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:-.}
fail(){ echo "journal-transport-codec-source: $*" >&2; exit 1; }
header=$root/include/libpkgapply/journal_transport_codec.h
source=$root/src/journal_transport_codec.cpp
test_source=$root/tests/protocol/journal_transport_codec_test.cpp
[ -s "$header" ] || fail 'owner transport codec header is absent'
[ -s "$source" ] || fail 'owner transport codec source is absent'
[ -s "$test_source" ] || fail 'owner transport codec protocol test is absent'
for value in declaration step cursor; do
  grep -F "encode_application_journal_${value}" "$header" >/dev/null || \
    fail "missing ${value} encoder"
  grep -F "decode_application_journal_${value}" "$header" >/dev/null || \
    fail "missing ${value} decoder"
done
# The append-only transport codec is its own owner protocol. It must not encode
# a synthetic complete-snapshot record and call the retired snapshot codec.
if grep -E 'application_journal_record|encode_application_journal\(|decode_application_journal\(' \
    "$source" >/dev/null; then
  fail 'append-only transport codec is implemented through legacy snapshot bytes'
fi
# Durable bytes must reproduce the semantic identity rather than trusting the
# filename/store locator.
grep -F 'identity_mismatch' "$source" >/dev/null || \
  fail 'transport decoder does not fail closed on identity mismatch'
for vector in \
  'declaration transport wire-format vector changed' \
  'step transport wire-format vector changed' \
  'cursor transport wire-format vector changed'; do
  grep -F "$vector" "$test_source" >/dev/null || \
    fail "missing protocol vector witness: $vector"
done
