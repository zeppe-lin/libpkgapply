#!/bin/sh
set -eu
root=$1
fail(){ echo "backend-authority-contract: $*" >&2; exit 1; }

admission="$root/src/admission.cpp"
engine="$root/src/application_engine.cpp"
restart="$root/src/restart.cpp"
backend_header="$root/include/libpkgapply/backend.h"
apply_header="$root/include/libpkgapply/apply.h"

for f in "$admission" "$engine" "$restart" "$backend_header" "$apply_header"; do
  [ -s "$f" ] || fail "missing ${f#$root/}"
done

grep -F 'validate_backend(target, backend);' "$admission" >/dev/null ||
  fail 'backend transaction admission does not revalidate the provider'
grep -F 'transaction.backend() != target.mutation_backend()' "$admission" >/dev/null ||
  fail 'transaction mutation backend is not target-bound'
grep -F 'transaction.observation_backend() != target.observation_backend()' "$admission" >/dev/null ||
  fail 'transaction observation backend is not target-bound'
grep -F 'transaction.capabilities() != target.capabilities()' "$admission" >/dev/null ||
  fail 'transaction capability evidence is not target-bound'
grep -F 'header.backend() != request.target().mutation_backend()' "$restart" >/dev/null ||
  fail 'restart journal backend is not request-target-bound'

if grep -F 'transaction.capabilities() != backend.capabilities()' "$admission" >/dev/null; then
  fail 'transaction capability evidence is compared to a fresh backend callback'
fi
if grep -F 'header.backend() != backend.identity()' "$restart" >/dev/null; then
  fail 'restart journal backend is compared to a fresh backend callback'
fi


# Semantic journal persistence is owner/store authority, never a mutation-backend
# virtual or durability command.
if grep -F 'publish_journal' "$backend_header" >/dev/null; then
  fail 'mutation backend still owns semantic journal persistence'
fi
if grep -F 'synchronize_journal' "$root/include/libpkgapply/journal.h" >/dev/null; then
  fail 'journal synchronization remains an application effect vocabulary token'
fi
grep -F 'application_journal_store& journal_store' "$apply_header" >/dev/null ||
  fail 'public application facade does not receive the journal store explicitly'
grep -F 'store.publish_declaration(intended)' "$engine" >/dev/null ||
  fail 'fresh execution does not publish owner declaration through the journal store'
grep -F 'store_->publish_step(intended)' "$engine" >/dev/null ||
  fail 'live execution does not append immutable journal steps through the store'
grep -F 'store_->compare_and_publish_cursor(previous.identity(), candidate)' "$engine" >/dev/null ||
  fail 'live execution does not durably advance the bounded journal cursor'
if grep -F 'synchronize(application_durability_domain::journal' "$engine" >/dev/null; then
  fail 'mutation transaction still synchronizes journal durability'
fi

# Attempts and journal headers must be derived from immutable request authority.
count=$(grep -F 'request.target().mutation_backend()' "$engine" | wc -l | tr -d ' ')
[ "$count" -ge 3 ] || fail 'engine does not derive attempt/journal backend from target authority'

echo 'backend-authority-contract: ok'
