#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu
root=$1

fail()
{
  echo "application-receipt-codec-source-test: $*" >&2
  exit 1
}

header=$root/include/libpkgapply/application_receipt_codec.h
source=$root/src/application_receipt_codec.cpp
test_source=$root/tests/application_receipt_codec_test.cpp

for file in "$header" "$source" "$test_source"; do
  test -s "$file" || fail "missing or empty ${file#"$root"/}"
done

for contract in \
  'application_receipt_encoding_version = 1' \
  'maximum_application_receipt_encoding_size' \
  'encode_application_receipt(' \
  'decode_application_receipt('
do
  grep -F "$contract" "$header" >/dev/null ||
    fail "public codec header omits $contract"
done

for contract in \
  'encode_completed_application_evidence(' \
  'decode_completed_application_evidence(' \
  'application_receipt::completed(' \
  'application_receipt::failed(' \
  'encode_application_receipt(result)' \
  'checksum_mismatch' \
  'request_mismatch'
do
  grep -F "$contract" "$source" >/dev/null ||
    fail "codec implementation omits $contract"
done

for forbidden in \
  'open(' \
  'openat(' \
  'stat(' \
  'lstat(' \
  'readlink(' \
  'resume_application(' \
  'apply_package('
do
  if grep -F "$forbidden" "$source" >/dev/null; then
    fail "codec implementation imports effectful operation $forbidden"
  fi
done

for meson_path in \
  src/application_receipt_codec.cpp \
  include/libpkgapply/application_receipt_codec.h \
  tests/application_receipt_codec_header_test.cpp \
  tests/application_receipt_codec_test.cpp \
  tests/application_receipt_codec_source_test.sh
do
  test -f "$root/$meson_path" || fail "Meson-referenced path is absent: $meson_path"
done

grep -F "'application_receipt_codec.cpp'" "$root/src/meson.build" >/dev/null ||
  fail 'core source list omits application receipt codec'
grep -F "'application_receipt_codec_test.cpp'" "$root/tests/meson.build" >/dev/null ||
  fail 'test source list omits application receipt codec test'
