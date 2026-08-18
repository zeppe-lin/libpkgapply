#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:-.}
fail(){ echo "journal-transport-source: $*" >&2; exit 1; }
header=$root/include/libpkgapply/journal_transport.h
source=$root/src/journal_transport.cpp
[ -s "$header" ] || fail 'append-only journal transport header is absent'
[ -s "$source" ] || fail 'append-only journal transport source is absent'
for required in \
  'class PKGAPPLY_API application_journal_declaration final' \
  'class PKGAPPLY_API application_journal_step final' \
  'class PKGAPPLY_API application_journal_cursor final' \
  'class PKGAPPLY_API application_journal_store' \
  'load_step(const application_journal_declaration_identity& declaration,' \
  'compare_and_publish_cursor(' \
  'application_journal_replay_encoding replay_fact'; do
  grep -F "$required" "$header" >/dev/null || fail "missing protocol authority: $required"
done
# A cursor is a bounded locator, never a hidden complete history snapshot.
block=$(sed -n '/class PKGAPPLY_API application_journal_cursor final/,/^};/p' "$header")
printf '%s\n' "$block" | grep -F 'std::vector<application_journal_event>' >/dev/null &&
  fail 'cursor retained complete event history'
printf '%s\n' "$block" | grep -F 'std::vector<application_journal_effect>' >/dev/null &&
  fail 'cursor retained complete effect graph'
# The store gets exact sequence addressing; directory enumeration is not semantic replay.
grep -F 'load_step(' "$header" >/dev/null || fail 'exact sequence load is absent'
