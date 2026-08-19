#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "restart-rehydration-source-contract: $*" >&2; exit 1; }
header=$root/include/libpkgapply/restart.h
source=$root/src/restart.cpp

grep -F 'rehydrate_application_journal(' "$header" >/dev/null || fail 'public owner rehydration entry point is absent'
grep -F 'application_journal_store& journal_store' "$header" >/dev/null || fail 'rehydration does not consume journal-store authority'
grep -F 'application_journal_declaration_identity& declaration' "$header" >/dev/null || fail 'rehydration is not exact-declaration addressed'
grep -F '#include "journal_history.h"' "$source" >/dev/null || fail 'restart implementation does not bind private history validator directly'
grep -F 'detail::application_journal_history::load(' "$source" >/dev/null || fail 'public rehydration bypasses owner history loader'
grep -F 'journal_store, declaration).snapshot();' "$source" >/dev/null || fail 'rehydration does not return owner-derived in-memory snapshot'
grep -F 'rehydrate_application_journal' "$root/ci/installed-core-consumer.cpp" >/dev/null || fail 'installed consumer does not link the owner rehydration entry point'
if grep -F 'libpkgapply-posix' "$header" "$source" >/dev/null; then
  fail 'mechanism provider leaked into owner rehydration'
fi
if grep -R -F 'rehydrate_application_journal' "$root/src/journal_transport_codec.cpp" "$root/src/journal_codec.cpp" >/dev/null; then
  fail 'rehydration was smuggled into durable codec authority'
fi
