# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if get_option('tests').allowed()
  sh = find_program('sh')
  test_inc = include_directories('.')
  core_inc = include_directories('../src')

  foreach case : [
    ['digest', 'unit/digest_test.cpp'],
    ['attempt', 'unit/attempt_test.cpp'],
    ['execution-control', 'unit/execution_control_test.cpp'],
    ['backend-model', 'unit/backend_model_test.cpp'],
    ['capture', 'unit/capture_test.cpp'],
    ['journal', 'unit/journal_test.cpp'],
    ['mutation-lease', 'unit/mutation_lease_test.cpp'],
    ['object-fact', 'unit/object_fact_test.cpp'],
    ['path-consequence', 'unit/path_consequence_test.cpp'],
    ['precondition', 'unit/precondition_test.cpp'],
    ['result', 'unit/result_test.cpp'],
    ['restart', 'unit/restart_test.cpp'],
    ['schedule', 'unit/schedule_test.cpp'],
    ['state-projection', 'unit/state_projection_test.cpp'],
    ['target-context', 'unit/target_context_test.cpp'],
  ]
    target = executable(
      case[0] + '-test',
      case[1],
      include_directories: test_inc,
      dependencies: libpkgapply_dep,
      install: false,
    )
    test(case[0], target, suite: 'unit')
  endforeach

  foreach case : [
    ['incoming-package', ['integration/incoming_package_test.cpp']],
    ['request', ['integration/request_test.cpp']],
    ['payload', ['integration/payload_test.cpp']],
    ['backend-authority', ['integration/backend_authority_test.cpp']],
    ['admission', [
      'integration/admission_test.cpp',
      'support/scripted_backend.cpp',
    ]],
    ['scripted-backend', [
      'integration/scripted_backend_test.cpp',
      'support/scripted_backend.cpp',
    ]],
    ['application-vertical', [
      'integration/application_vertical_test.cpp',
      'support/scripted_backend.cpp',
    ]],
  ]
    target = executable(
      case[0] + '-test',
      case[1],
      include_directories: [test_inc, core_inc],
      dependencies: libpkgapply_dep,
      install: false,
    )
    test(case[0], target, suite: 'integration')
  endforeach

  foreach case : [
    ['canonical-record', 'protocol/canonical_record_test.cpp'],
    ['journal-codec', 'protocol/journal_codec_test.cpp'],
    ['application-receipt-codec', 'protocol/application_receipt_codec_test.cpp'],
    ['completed-evidence-codec', 'protocol/completed_evidence_codec_test.cpp'],
    ['restart-checkpoint-codec', 'protocol/restart_checkpoint_codec_test.cpp'],
  ]
    target = executable(
      case[0] + '-test',
      case[1],
      include_directories: [test_inc, core_inc],
      dependencies: [libpkgapply_dep, libcrypto_dep],
      install: false,
    )
    test(case[0], target, suite: 'protocol')
  endforeach

  public_headers = [
    'admission.h',
    'application_receipt_codec.h',
    'apply.h',
    'attempt.h',
    'backend.h',
    'capture.h',
    'completed_evidence_codec.h',
    'digest.h',
    'execution_control.h',
    'incoming_package.h',
    'journal.h',
    'journal_codec.h',
    'libpkgapply.h',
    'mutation_lease.h',
    'object_fact.h',
    'path_consequence.h',
    'payload.h',
    'precondition.h',
    'request.h',
    'restart.h',
    'restart_checkpoint_codec.h',
    'result.h',
    'schedule.h',
    'state_projection.h',
    'target_context.h',
    'version.h',
    'export.h',
  ]
  foreach header : public_headers
    target = executable(
      'header-' + header.underscorify(),
      'header/public_header_test.cpp',
      cpp_args: ['-DPKGAPPLY_TEST_HEADER="libpkgapply/' + header + '"'],
      dependencies: libpkgapply_dep,
      install: false,
    )
    test('header-' + header.underscorify(), target, suite: 'header')
  endforeach


  if library_kind == 'shared'
    nm = find_program('nm')
    abi_surface = find_program('contracts/check_abi_surface.sh')
    test(
      'abi-surface',
      abi_surface,
      args: [
        nm.full_path(),
        libpkgapply,
        meson.project_source_root() / 'abi/libpkgapply.exports',
      ],
      suite: 'contract',
    )
  endif

  test(
    'pkgconfig-metadata',
    sh,
    args: [
      meson.current_source_dir() / 'contracts/check_pkgconfig_metadata.sh',
      meson.project_build_root(),
    ],
    suite: 'contract',
  )

  foreach contract : [
    'application_receipt_codec_source',
    'mutation_lease_scope_source',
    'ci_qualification_source',
    'release_metadata',
    'architecture_contract',
    'backend_authority_contract',
    'abi_contract',
    'repository_contract',
    'test_layout',
  ]
    test(
      contract.replace('_', '-'),
      sh,
      args: [
        meson.current_source_dir() / ('contracts/check_' + contract + '.sh'),
        meson.project_source_root(),
      ],
      suite: 'contract',
    )
  endforeach

  # The Doxygen parser compiles installed public headers in isolation.  Pass
  # the complete public dependency include closure explicitly so qualification
  # cannot borrow transitive zoo headers from an ambient installation prefix.
  libpkgbuild_image_test_dep = dependency('libpkgbuild-image', required: true, static: want_static)
  libpkgbuild_test_dep = dependency('libpkgbuild', required: true, static: want_static)
  libpkgimage_test_dep = dependency('libpkgimage', required: true, static: want_static)
  libpkgsource_plan_test_dep = dependency('libpkgsource-plan', required: true, static: want_static)
  libpkgsource_test_dep = dependency('libpkgsource', required: true, static: want_static)
  libpkgresolve_test_dep = dependency('libpkgresolve', required: true, static: want_static)
  libpkgcatalog_test_dep = dependency('libpkgcatalog', required: true, static: want_static)
  libpkgstate_test_dep = dependency('libpkgstate', required: true, static: want_static)

  libpkgbuild_plan_public_include = libpkgbuild_plan_dep.get_variable(
    pkgconfig: 'includedir',
  )
  libpkgplan_public_include = libpkgplan_dep.get_variable(
    pkgconfig: 'includedir',
  )
  libpkgbuild_image_public_include = libpkgbuild_image_test_dep.get_variable(pkgconfig: 'includedir')
  libpkgbuild_public_include = libpkgbuild_test_dep.get_variable(pkgconfig: 'includedir')
  libpkgimage_public_include = libpkgimage_test_dep.get_variable(pkgconfig: 'includedir')
  libpkgsource_plan_public_include = libpkgsource_plan_test_dep.get_variable(pkgconfig: 'includedir')
  libpkgsource_public_include = libpkgsource_test_dep.get_variable(pkgconfig: 'includedir')
  libpkgresolve_public_include = libpkgresolve_test_dep.get_variable(pkgconfig: 'includedir')
  libpkgcatalog_public_include = libpkgcatalog_test_dep.get_variable(pkgconfig: 'includedir')
  libpkgstate_public_include = libpkgstate_test_dep.get_variable(pkgconfig: 'includedir')
  test(
    'documentation-source',
    sh,
    args: [
      meson.current_source_dir() / 'contracts/check_documentation_source.sh',
      meson.project_source_root(),
      libpkgbuild_plan_public_include,
      libpkgplan_public_include,
      libpkgbuild_image_public_include,
      libpkgbuild_public_include,
      libpkgimage_public_include,
      libpkgsource_plan_public_include,
      libpkgsource_public_include,
      libpkgresolve_public_include,
      libpkgcatalog_public_include,
      libpkgstate_public_include,
    ],
    suite: 'contract',
  )

  test(
    'style-contract',
    sh,
    args: [
      meson.current_source_dir() / 'contracts/check_style_contract.sh',
      meson.project_source_root(),
      meson.project_build_root(),
    ],
    suite: 'contract',
  )
endif
