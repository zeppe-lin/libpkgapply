# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

public_inc = include_directories('../include')

libpkgapply = library(
  'pkgapply',
  [
    'version.cpp',
    'digest.cpp',
    'sha256.cpp',
    'canonical_record.cpp',
  ],
  include_directories: public_inc,
  dependencies: libcrypto_dep,
  install: true,
  version: meson.project_version(),
  soversion: '0',
)

libpkgapply_dep = declare_dependency(
  link_with: libpkgapply,
  include_directories: public_inc,
  dependencies: libcrypto_dep,
)

meson.override_dependency('libpkgapply', libpkgapply_dep)

install_headers(
  '../include/libpkgapply/libpkgapply.h',
  '../include/libpkgapply/digest.h',
  '../include/libpkgapply/version.h',
  subdir: 'libpkgapply',
)

pkgconf.generate(
  libpkgapply,
  name: 'libpkgapply',
  filebase: 'libpkgapply',
  description: 'Package operation application library',
  subdirs: '.',
  requires_private: [libcrypto_dep],
)
