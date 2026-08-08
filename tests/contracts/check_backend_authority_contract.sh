#!/bin/sh
set -eu
root=$1
fail(){ echo "backend-authority-contract: $*" >&2; exit 1; }

admission="$root/src/admission.cpp"
engine="$root/src/application_engine.cpp"
restart="$root/src/restart.cpp"

for f in "$admission" "$engine" "$restart"; do
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

# Attempts and journal headers must be derived from immutable request authority.
count=$(grep -F 'request.target().mutation_backend()' "$engine" | wc -l | tr -d ' ')
[ "$count" -ge 3 ] || fail 'engine does not derive attempt/journal backend from target authority'

echo 'backend-authority-contract: ok'
