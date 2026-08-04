# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if get_option('tests').allowed()
  sh = find_program('sh')

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

  attempt_header_test = executable(
    'attempt-header-test',
    'attempt_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile attempt header independently', attempt_header_test)

  attempt_test = executable(
    'attempt-test',
    'attempt_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate backend-bound application attempts', attempt_test)

  incoming_package_header_test = executable(
    'incoming-package-header-test',
    'incoming_package_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test(
    'compile incoming package header independently',
    incoming_package_header_test,
  )

  incoming_package_test = executable(
    'incoming-package-test',
    'incoming_package_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate native incoming build authority', incoming_package_test)

  admission_header_test = executable(
    'admission-header-test',
    'admission_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile admission header independently', admission_header_test)

  apply_header_test = executable(
    'apply-header-test',
    'apply_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile apply header independently', apply_header_test)

  application_engine_test = executable(
    'application-engine-test',
    [
      'application_engine_test.cpp',
      'scripted_backend.cpp',
    ],
    include_directories: include_directories('../src'),
    dependencies: libpkgapply_dep,
  )
  test(
    'execute application engine through active effects',
    application_engine_test,
  )

  admission_test = executable(
    'admission-test',
    [
      'admission_test.cpp',
      'scripted_backend.cpp',
    ],
    dependencies: libpkgapply_dep,
  )
  test('validate application authority admission', admission_test)

  backend_header_test = executable(
    'backend-header-test',
    'backend_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile backend header independently', backend_header_test)

  backend_model_test = executable(
    'backend-model-test',
    'backend_model_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate constrained backend mechanism values', backend_model_test)

  scripted_backend_test = executable(
    'scripted-backend-test',
    [
      'scripted_backend.cpp',
      'scripted_backend_test.cpp',
    ],
    dependencies: libpkgapply_dep,
  )
  test('exercise deterministic backend failure injection', scripted_backend_test)

  result_header_test = executable(
    'result-header-test',
    'result_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile result header independently', result_header_test)

  result_test = executable(
    'result-test',
    'result_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate application receipts and completed evidence', result_test)

  object_fact_header_test = executable(
    'object-fact-header-test',
    'object_fact_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile object fact header independently', object_fact_header_test)

  path_consequence_header_test = executable(
    'path-consequence-header-test',
    'path_consequence_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile path consequence header independently', path_consequence_header_test)

  path_consequence_test = executable(
    'path-consequence-test',
    'path_consequence_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate application path consequences', path_consequence_test)

  precondition_header_test = executable(
    'precondition-header-test',
    'precondition_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile precondition header independently', precondition_header_test)

  precondition_test = executable(
    'precondition-test',
    'precondition_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('revalidate planning-time filesystem preconditions', precondition_test)

  payload_header_test = executable(
    'payload-header-test',
    'payload_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile payload header independently', payload_header_test)

  payload_test = executable(
    'payload-test',
    'payload_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('derive exact incoming regular payload closure', payload_test)

  capture_header_test = executable(
    'capture-header-test',
    'capture_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile capture header independently', capture_header_test)

  capture_test = executable(
    'capture-test',
    'capture_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('derive old-object rejected and recovery captures', capture_test)

  schedule_header_test = executable(
    'schedule-header-test',
    'schedule_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile schedule header independently', schedule_header_test)

  schedule_test = executable(
    'schedule-test',
    'schedule_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('derive deterministic safe application order', schedule_test)

  object_fact_test = executable(
    'object-fact-test',
    'object_fact_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate completed object facts', object_fact_test)

  state_projection_header_test = executable(
    'state-projection-header-test',
    'state_projection_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile state projection header independently', state_projection_header_test)

  state_projection_test = executable(
    'state-projection-test',
    'state_projection_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate lease-bound state projections', state_projection_test)

  mutation_lease_header_test = executable(
    'mutation-lease-header-test',
    'mutation_lease_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile mutation lease header independently', mutation_lease_header_test)

  mutation_lease_test = executable(
    'mutation-lease-test',
    'mutation_lease_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate caller-held target mutation leases', mutation_lease_test)


  journal_codec_header_test = executable(
    'journal-codec-header-test',
    'journal_codec_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile journal codec header independently', journal_codec_header_test)

  journal_codec_test = executable(
    'journal-codec-test',
    'journal_codec_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('round-trip durable journal encodings', journal_codec_test)



  journal_header_test = executable(
    'journal-header-test',
    'journal_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile journal header independently', journal_header_test)

  journal_test = executable(
    'journal-test',
    'journal_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate durable application journal records', journal_test)

  execution_control_header_test = executable(
    'execution-control-header-test',
    'execution_control_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile execution control header independently', execution_control_header_test)

  restart_header_test = executable(
    'restart-header-test',
    'restart_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile restart header independently', restart_header_test)

  restart_test = executable(
    'restart-test',
    'restart_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('classify durable application restart journals', restart_test)

  request_header_test = executable(
    'request-header-test',
    'request_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile request header independently', request_header_test)

  request_test = executable(
    'request-test',
    'request_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('validate immutable application requests', request_test)

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

  application_receipt_codec_header_test = executable(
    'application-receipt-codec-header-test',
    'application_receipt_codec_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test(
    'compile application receipt codec header independently',
    application_receipt_codec_header_test,
  )

  application_receipt_codec_test = executable(
    'application-receipt-codec-test',
    'application_receipt_codec_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('encode durable application receipts', application_receipt_codec_test)

  completed_evidence_codec_header_test = executable(
    'completed-evidence-codec-header-test',
    'completed_evidence_codec_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test(
    'compile completed evidence codec header independently',
    completed_evidence_codec_header_test,
  )
  restart_checkpoint_codec_header_test = executable(
    'restart-checkpoint-codec-header-test',
    'restart_checkpoint_codec_header_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('compile restart checkpoint codec header independently', restart_checkpoint_codec_header_test)

  restart_checkpoint_codec_test = executable(
    'restart-checkpoint-codec-test',
    'restart_checkpoint_codec_test.cpp',
    dependencies: libpkgapply_dep,
  )
  test('encode durable restart checkpoints', restart_checkpoint_codec_test)


  test(
    'verify durable application receipt codec contract',
    sh,
    args: [
      join_paths(meson.current_source_dir(),
                 'application_receipt_codec_source_test.sh'),
      meson.project_source_root(),
    ],
  )

  test(
    'verify target-scoped mutation lease contract',
    sh,
    args: [
      join_paths(meson.current_source_dir(),
                 'mutation_lease_scope_source_test.sh'),
      meson.project_source_root(),
    ],
  )


  test(
    'verify documentation source contracts',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'documentation_source_test.sh'),
      meson.project_source_root(),
    ],
  )

  test(
    'verify CI qualification sources',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'ci_qualification_source_test.sh'),
      meson.project_source_root(),
    ],
  )

  test(
    'verify release metadata',
    sh,
    args: [
      join_paths(meson.current_source_dir(), 'release_metadata_test.sh'),
      meson.project_source_root(),
    ],
  )

  test(
    'contract architecture-contract',
    sh,
    args: [join_paths(meson.current_source_dir(), 'contracts/check_architecture_contract.sh'), meson.project_source_root()],
  )

  test(
    'contract abi-contract',
    sh,
    args: [join_paths(meson.current_source_dir(), 'contracts/check_abi_contract.sh'), meson.project_source_root()],
  )

  test(
    'contract repository-contract',
    sh,
    args: [join_paths(meson.current_source_dir(), 'contracts/check_repository_contract.sh'), meson.project_source_root()],
  )

  test(
    'contract style-contract',
    sh,
    args: [join_paths(meson.current_source_dir(), 'contracts/check_style_contract.sh'), meson.project_source_root()],
  )

endif
