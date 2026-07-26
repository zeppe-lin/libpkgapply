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
    'attempt.cpp',
    'apply.cpp',
    'application_engine.cpp',
    'admission.cpp',
    'backend.cpp',
    'capture.cpp',
    'completed_evidence_codec.cpp',
    'target_context.cpp',
    'execution_control.cpp',
    'incoming_package.cpp',
    'request.cpp',
    'restart.cpp',
    'restart_checkpoint_codec.cpp',
    'state_projection.cpp',
    'mutation_lease.cpp',
    'journal.cpp',
    'journal_codec.cpp',
    'object_fact.cpp',
    'path_consequence.cpp',
    'payload.cpp',
    'precondition.cpp',
    'result.cpp',
    'schedule.cpp',
  ],
  include_directories: public_inc,
  dependencies: [
    libcrypto_dep,
    libpkgbuild_dep,
    libpkgimage_dep,
    libpkgplan_dep,
    libpkgsource_plan_dep,
  ],
  install: true,
  version: meson.project_version(),
  soversion: '1',
)

libpkgapply_dep = declare_dependency(
  link_with: libpkgapply,
  include_directories: public_inc,
  dependencies: [
    libcrypto_dep,
    libpkgbuild_dep,
    libpkgimage_dep,
    libpkgplan_dep,
    libpkgsource_plan_dep,
  ],
)

meson.override_dependency('libpkgapply', libpkgapply_dep)

install_headers(
  '../include/libpkgapply/libpkgapply.h',
  '../include/libpkgapply/digest.h',
  '../include/libpkgapply/attempt.h',
  '../include/libpkgapply/apply.h',
  '../include/libpkgapply/admission.h',
  '../include/libpkgapply/backend.h',
  '../include/libpkgapply/capture.h',
  '../include/libpkgapply/completed_evidence_codec.h',
  '../include/libpkgapply/target_context.h',
  '../include/libpkgapply/execution_control.h',
  '../include/libpkgapply/incoming_package.h',
  '../include/libpkgapply/request.h',
  '../include/libpkgapply/restart.h',
  '../include/libpkgapply/restart_checkpoint_codec.h',
  '../include/libpkgapply/state_projection.h',
  '../include/libpkgapply/mutation_lease.h',
  '../include/libpkgapply/journal.h',
  '../include/libpkgapply/journal_codec.h',
  '../include/libpkgapply/object_fact.h',
  '../include/libpkgapply/path_consequence.h',
  '../include/libpkgapply/payload.h',
  '../include/libpkgapply/precondition.h',
  '../include/libpkgapply/result.h',
  '../include/libpkgapply/schedule.h',
  '../include/libpkgapply/version.h',
  subdir: 'libpkgapply',
)

pkgconf.generate(
  libpkgapply,
  name: 'libpkgapply',
  filebase: 'libpkgapply',
  description: 'Package operation application library',
  subdirs: '.',
  requires: [
    'libpkgbuild >= 1.0.0',
    'libpkgimage >= 0.3.0',
    'libpkgplan >= 0.2.0',
    'libpkgsource-plan >= 1.0.0',
  ],
  requires_private: [libcrypto_dep],
)
