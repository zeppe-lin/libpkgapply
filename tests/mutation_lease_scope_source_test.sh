#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu
root=$1

fail()
{
  echo "mutation-lease-scope-source-test: $*" >&2
  exit 1
}

header=$root/include/libpkgapply/mutation_lease.h
source=$root/src/mutation_lease.cpp

test -s "$header" || fail 'missing mutation lease public header'
test -s "$source" || fail 'missing mutation lease implementation'

grep -F 'void validate_target_mutation_lease_scope(' "$header" >/dev/null ||
  fail 'public header omits target-scope validator'
grep -F 'void' "$source" >/dev/null ||
  fail 'mutation lease implementation is empty'
grep -F 'validate_target_mutation_lease_scope(' "$source" >/dev/null ||
  fail 'implementation omits target-scope validator'
grep -F 'validate_target_mutation_lease_scope(target, lease);' "$source" >/dev/null ||
  fail 'full validator does not reuse target-scope validation'

grep -F 'lease.target() != target.identity()' "$source" >/dev/null ||
  fail 'scope validator omits exact target validation'
grep -F 'lease.exclusion_domain() != target.mutation_exclusion_domain()' \
  "$source" >/dev/null ||
  fail 'scope validator omits exclusion-domain validation'

scope_body=$(sed -n \
  '/^validate_target_mutation_lease_scope(/,/^validate_target_mutation_lease(/p' \
  "$source")
case $scope_body in
  *'state.lease()'*) fail 'scope validator imports state-projection authority' ;;
esac

if grep -F 'libpkgstate' "$header" "$source" >/dev/null; then
  fail 'core lease validation is contaminated by libpkgstate'
fi
