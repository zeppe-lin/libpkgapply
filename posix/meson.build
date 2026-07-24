# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

libpkgapply_posix = library(
  'pkgapply-posix',
  ['journal_store.cpp'],
  include_directories: public_inc,
  dependencies: libpkgapply_dep,
  install: true,
  version: meson.project_version(),
  soversion: '0',
)

libpkgapply_posix_dep = declare_dependency(
  link_with: libpkgapply_posix,
  include_directories: public_inc,
  dependencies: libpkgapply_dep,
)

meson.override_dependency('libpkgapply-posix', libpkgapply_posix_dep)

install_headers(
  '../include/libpkgapply-posix/libpkgapply-posix.h',
  '../include/libpkgapply-posix/journal_store.h',
  subdir: 'libpkgapply-posix',
)

pkgconf.generate(
  libpkgapply_posix,
  name: 'libpkgapply-posix',
  filebase: 'libpkgapply-posix',
  description: 'POSIX mechanisms for libpkgapply',
  subdirs: '.',
  requires: ['libpkgapply >= ' + meson.project_version()],
)
