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


  posix_public_header_test = executable(
    'posix-public-header-test',
    'posix_public_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('compile POSIX public umbrella header', posix_public_header_test)

  posix_journal_store_header_test = executable(
    'posix-journal-store-header-test',
    'posix_journal_store_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test(
    'compile POSIX journal store header independently',
    posix_journal_store_header_test,
  )

  posix_journal_store_test = executable(
    'posix-journal-store-test',
    'posix_journal_store_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('persist POSIX journal snapshots atomically', posix_journal_store_test)

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

  posix_capture_store_header_test = executable(
    'posix-capture-store-header-test',
    'posix_capture_store_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test(
    'compile POSIX capture store header independently',
    posix_capture_store_header_test,
  )

  posix_capture_store_test = executable(
    'posix-capture-store-test',
    'posix_capture_store_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('capture private POSIX old objects', posix_capture_store_test)


  posix_completed_evidence_store_header_test = executable(
    'posix-completed-evidence-store-header-test',
    'posix_completed_evidence_store_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test(
    'compile POSIX completed evidence store header independently',
    posix_completed_evidence_store_header_test,
  )

  posix_completed_evidence_store_test = executable(
    'posix-completed-evidence-store-test',
    'posix_completed_evidence_store_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test(
    'publish immutable POSIX completed evidence',
    posix_completed_evidence_store_test,
  )

  posix_checkpoint_store_header_test = executable(
    'posix-checkpoint-store-header-test',
    'posix_checkpoint_store_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('compile POSIX checkpoint store header independently', posix_checkpoint_store_header_test)

  posix_checkpoint_store_test = executable(
    'posix-checkpoint-store-test',
    'posix_checkpoint_store_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('persist durable restart checkpoints', posix_checkpoint_store_test)

  posix_payload_stage_header_test = executable(
    'posix-payload-stage-header-test',
    'posix_payload_stage_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test(
    'compile POSIX payload stage header independently',
    posix_payload_stage_header_test,
  )

  posix_payload_stage_test = executable(
    'posix-payload-stage-test',
    'posix_payload_stage_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('stage private POSIX incoming payloads', posix_payload_stage_test)

  posix_rejected_store_header_test = executable(
    'posix-rejected-store-header-test',
    'posix_rejected_store_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test(
    'compile POSIX rejected store header independently',
    posix_rejected_store_header_test,
  )

  posix_rejected_store_test = executable(
    'posix-rejected-store-test',
    'posix_rejected_store_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('publish immutable POSIX rejected objects', posix_rejected_store_test)

  posix_target_observer_header_test = executable(
    'posix-target-observer-header-test',
    'posix_target_observer_header_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('compile POSIX target observer header independently', posix_target_observer_header_test)

  posix_target_observer_test = executable(
    'posix-target-observer-test',
    'posix_target_observer_test.cpp',
    dependencies: libpkgapply_posix_dep,
  )
  test('observe FD-anchored POSIX target objects', posix_target_observer_test)

  posix_active_workspace_test = executable(
    'posix-active-workspace-test',
    'posix_active_workspace_test.cpp',
    include_directories: include_directories('../posix'),
    dependencies: libpkgapply_posix_dep,
  )
  test('classify attempt-bound POSIX active workspaces',
       posix_active_workspace_test)

  posix_active_incoming_test = executable(
    'posix-active-incoming-test',
    'posix_active_incoming_test.cpp',
    include_directories: include_directories('../posix'),
    dependencies: libpkgapply_posix_dep,
  )
  test('publish incoming POSIX active objects', posix_active_incoming_test)

  posix_active_removal_test = executable(
    'posix-active-removal-test',
    'posix_active_removal_test.cpp',
    include_directories: include_directories('../posix'),
    dependencies: libpkgapply_posix_dep,
  )
  test('remove POSIX active objects', posix_active_removal_test)

  posix_active_recovery_test = executable(
    'posix-active-recovery-test',
    'posix_active_recovery_test.cpp',
    include_directories: include_directories('../posix'),
    dependencies: libpkgapply_posix_dep,
  )
  test('restore POSIX active objects', posix_active_recovery_test)

endif
