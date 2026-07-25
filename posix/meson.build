# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

libpkgapply_posix = library(
  'pkgapply-posix',
  [
    'active_namespace.cpp',
    'active_workspace.cpp',
    'capture_store.cpp',
    'completed_evidence_store.cpp',
    'checkpoint_store.cpp',
    'journal_store.cpp',
    'payload_stage.cpp',
    'rejected_store.cpp',
    'target_observer.cpp',
  ],
  include_directories: public_inc,
  dependencies: [libpkgapply_dep, libcrypto_dep],
  install: true,
  version: meson.project_version(),
  soversion: '0',
)

libpkgapply_posix_dep = declare_dependency(
  link_with: libpkgapply_posix,
  include_directories: public_inc,
  dependencies: [libpkgapply_dep, libcrypto_dep],
)

meson.override_dependency('libpkgapply-posix', libpkgapply_posix_dep)

install_headers(
  '../include/libpkgapply-posix/libpkgapply-posix.h',
  '../include/libpkgapply-posix/capture_store.h',
  '../include/libpkgapply-posix/completed_evidence_store.h',
  '../include/libpkgapply-posix/checkpoint_store.h',
  '../include/libpkgapply-posix/journal_store.h',
  '../include/libpkgapply-posix/payload_stage.h',
  '../include/libpkgapply-posix/rejected_store.h',
  '../include/libpkgapply-posix/target_observer.h',
  subdir: 'libpkgapply-posix',
)

pkgconf.generate(
  libpkgapply_posix,
  name: 'libpkgapply-posix',
  filebase: 'libpkgapply-posix',
  description: 'POSIX mechanisms for libpkgapply',
  subdirs: '.',
  requires: ['libpkgapply >= ' + meson.project_version()],
  requires_private: [libcrypto_dep],
)
