#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:-.}
fail(){ echo "restart-view-source: $*" >&2; exit 1; }
header=$root/include/libpkgapply/restart.h
builder=$root/src/restart_view.cpp
engine=$root/src/application_engine.cpp
backend=$root/include/libpkgapply/backend.h
[ -s "$builder" ] || fail 'owner restart-view builder is absent'
grep -F 'class application_restart_view final' "$header" >/dev/null ||
  fail 'owner-derived public restart view is absent'
grep -F 'application_restart_view_builder::build(history, request)' "$engine" >/dev/null ||
  fail 'engine does not derive restart view from loaded owner history'
grep -F 'for (const auto& step : history.steps())' "$builder" >/dev/null ||
  fail 'restart view does not derive terminal facts from exact owner steps'
grep -F 'decode_replay_seed(history.declaration().replay_seed())' "$builder" >/dev/null ||
  fail 'restart view does not derive admitted observations from owner declaration'
grep -F 'decode_replay_fact(step.replay_fact(), request)' "$builder" >/dev/null ||
  fail 'restart view does not decode owner-authored transition facts'
grep -F 'history.effect(event.effect())' "$builder" >/dev/null ||
  fail 'restart view does not use indexed effect lookup'
grep -F 'using path_ordinal_index = std::unordered_map' "$builder" >/dev/null ||
  fail 'restart view does not index planned paths once'
grep -F 'backend_evidence_seen' "$builder" >/dev/null ||
  fail 'restart view does not deduplicate evidence incrementally'
for forbidden_lookup in 'std::find_if(' 'std::lower_bound(' 'std::sort('; do
  if grep -F "$forbidden_lookup" "$builder" >/dev/null; then
    fail "restart projection contains a superlinear per-history lookup: $forbidden_lookup"
  fi
done
grep -F 'const application_restart_view& restart' "$backend" >/dev/null ||
  fail 'backend reopen does not receive owner-derived view'
for forbidden in restart_checkpoint_codec 'restart_checkpoint(' 'resumed_journal(' reconcile_restart_checkpoint; do
  if grep -R -F "$forbidden" "$root/include/libpkgapply" "$root/src" >/dev/null; then
    fail "retired provider restart authority remains: $forbidden"
  fi
done
if grep -R -E 'encode_application_restart_view|decode_application_restart_view' "$root/include" "$root/src" >/dev/null; then
  fail 'ephemeral restart view acquired a durable codec'
fi
echo 'restart-view-source: ok'
