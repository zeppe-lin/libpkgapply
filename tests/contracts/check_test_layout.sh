#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$1
fail(){ echo "test-layout-contract: $*" >&2; exit 1; }

for directory in unit integration protocol header fixtures support contracts; do
  [ -d "$root/tests/$directory" ] || fail "missing tests/$directory"
done

[ -f "$root/tests/integration/application_vertical_test.cpp" ] ||
  fail 'composed application vertical is missing'
[ -f "$root/tests/integration/backend_authority_test.cpp" ] ||
  fail 'backend authority regression is missing'
[ -f "$root/tests/protocol/completed_evidence_codec_test.cpp" ] ||
  fail 'completed-evidence protocol qualification is missing'
[ -f "$root/tests/header/public_header_test.cpp" ] ||
  fail 'generic public-header harness is missing'

if find "$root/tests" -maxdepth 1 -type f \( -name '*.cpp' -o -name '*.h' \) | grep . >/dev/null; then
  fail 'C++ tests or fixtures remain in the tests root'
fi
if find "$root/tests" -maxdepth 1 -type f -name '*.sh' | grep . >/dev/null; then
  fail 'shell contracts remain in the tests root'
fi
if find "$root/tests" -maxdepth 1 -type f -name '*_header_test.cpp' | grep . >/dev/null; then
  fail 'legacy one-file header harness remains'
fi
if grep -n "test('header:" "$root/tests/meson.build" >/dev/null 2>&1; then
  fail 'deprecated colon appears in header test names'
fi
for suite in unit integration protocol header contract; do
  grep -F "suite: '$suite'" "$root/tests/meson.build" >/dev/null ||
    fail "Meson does not register the $suite suite"
done

echo 'test-layout-contract: ok'
