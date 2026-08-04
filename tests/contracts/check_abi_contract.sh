#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

[ "$#" -eq 1 ] || {
  echo "usage: $0 SOURCE-ROOT" >&2
  exit 2
}
root=$1

fail()
{
  echo "abi-contract: $*" >&2
  exit 1
}

for type in \
  mutation_lease_error \
  application_receipt_codec_error \
  application_journal_codec_error \
  application_journal_transition_error \
  application_admission_error \
  digest_error \
  incoming_package_error \
  completed_application_evidence_codec_error \
  application_restart_error \
  application_restart_checkpoint_codec_error
do
  grep -R -F "~$type() override;" "$root/include/libpkgapply" >/dev/null ||
    fail "public exception destructor is not declared: $type"
  grep -R -F "~$type() = default;" "$root/src" >/dev/null ||
    fail "public exception vtable is not anchored: $type"
done

for type in \
  target_mutation_lease \
  incoming_payload_stage \
  application_backend_transaction \
  application_backend
do
  grep -R -F "~$type() = default;" "$root/src" >/dev/null ||
    fail "abstract interface vtable is not anchored: $type"
done

grep -F 'exact core ABI capture' "$root/CHANGELOG.md" >/dev/null ||
  fail 'pre-tag exact ABI gate is undocumented'
