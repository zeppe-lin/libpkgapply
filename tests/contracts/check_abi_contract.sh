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

[ -s "$root/include/libpkgapply/export.h" ] || fail 'public export annotation header is absent'
[ -s "$root/abi/libpkgapply.exports" ] || fail 'reviewed ELF ABI manifest is absent'
[ "$(sed -n '/^_Z[A-Za-z0-9_]*$/p' "$root/abi/libpkgapply.exports" | wc -l)" -eq 730 ] ||
  fail 'reviewed ELF ABI manifest count changed without review'
for signature in \
  '16canonical_domainEv' \
  '5parseESt17basic_string_viewIcSt11char_traitsIcEE' \
  '9algorithmEv' \
  '6stringB5cxx11Ev' \
  '5bytesEv'
do
  [ "$(grep -c "typed_digest.*${signature}$" "$root/abi/libpkgapply.exports")" -eq 27 ] ||
    fail "public typed-digest ABI member is not exported for all domains: $signature"
done
[ "$(grep -c '^struct PKGAPPLY_API .*_identity_domain final {' "$root/include/libpkgapply/digest.h")" -eq 27 ] ||
  fail 'public typed-digest domain visibility is incomplete'
if grep -F '_ZN8pkgapply6detail16canonical_record' "$root/abi/libpkgapply.exports" >/dev/null; then
  fail 'private canonical-record implementation leaked into public ABI'
fi
if grep -F '_ZN8pkgapply6detail24admit_application_engine' "$root/abi/libpkgapply.exports" >/dev/null; then
  fail 'private application engine leaked into public ABI'
fi
[ -x "$root/tools/generate-elf-export-script.sh" ] || fail 'ELF export-script generator is absent'
grep -F "soversion: '3'" "$root/src/meson.build" >/dev/null || fail 'core SONAME is not generation 3'
grep -F 'api_version = 3' "$root/include/libpkgapply/version.h" >/dev/null || fail 'public API is not generation 3'
grep -F "gnu_symbol_visibility: 'hidden'" "$root/src/meson.build" >/dev/null || fail 'hidden default visibility is absent'
grep -F -- '-DPKGAPPLY_BUILDING_LIBRARY' "$root/src/meson.build" >/dev/null || fail 'library export annotation define is absent'
grep -F -- '--version-script=' "$root/src/meson.build" >/dev/null || fail 'ELF export manifest is not linked'
grep -F 'Advanced the core to SONAME 3 and public API generation 3' "$root/HISTORY.md" >/dev/null ||
  fail '3.0 ABI generation transition is undocumented'
grep -F '730 symbols' "$root/docs/abi.md" >/dev/null || fail 'reviewed ABI inventory is undocumented'


grep -F "'../src/canonical_record.cpp'" "$root/tests/meson.build" >/dev/null ||
  fail 'canonical-record protocol test does not link its private implementation'
grep -F "'../src/sha256.cpp'" "$root/tests/meson.build" >/dev/null ||
  fail 'canonical-record protocol test does not link its private digest provider'
grep -F "'../src/application_engine.cpp'" "$root/tests/meson.build" >/dev/null ||
  fail 'application vertical does not link its private engine implementation'
