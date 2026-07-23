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
endif
