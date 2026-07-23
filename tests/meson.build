# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if get_option('tests').allowed()
  public_header_test = executable(
    'public-header-test',
    'public_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile public umbrella header', public_header_test)

  version_header_test = executable(
    'version-header-test',
    'version_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile version header independently', version_header_test)

  digest_header_test = executable(
    'digest-header-test',
    'digest_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile digest header independently', digest_header_test)

  digest_test = executable(
    'digest-test',
    'digest_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate typed application identities', digest_test)


  target_context_header_test = executable(
    'target-context-header-test',
    'target_context_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile target context header independently', target_context_header_test)

  target_context_test = executable(
    'target-context-test',
    'target_context_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate target application contexts', target_context_test)

  canonical_record_test = executable(
    'canonical-record-test',
    'canonical_record_test.cpp',
    include_directories: include_directories('../src'),
    dependencies: libpkgapply_dep,
  )
  test('validate canonical application records', canonical_record_test)
endif
