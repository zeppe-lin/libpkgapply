3.0.0 - unreleased
------------------

Authority correction

- Replaced direct build, image, and source-projection admission with one opaque
  `libpkgbuild-plan` artifact projection.
- Delegated exact build/image agreement to `libpkgbuild-image` and candidate plus
  artifact projection to `libpkgbuild-plan`; application no longer reimplements
  either proof.
- Bound installation and upgrade requests to the exact upstream candidate,
  release, artifact, manifest, archive, image, inspection receipt, precondition,
  and publication named by the accepted plan.
- Collapsed the undeployed application-request and restart-checkpoint protocols
  to their first actual generation.
- Exposed only `libpkgbuild-plan` and `libpkgplan` through installed pkg-config
  metadata; retained OpenSSL as the private cryptographic requirement.
- Rebound fresh backend-transaction and restart-journal evidence to the immutable
  request target context; a provider cannot replace admitted mutation,
  observation, or capability authority with later callback values.
- Revalidated the selected backend before accepting its transaction and derive
  attempt/journal backend identities from request-bound target authority.
- Split qualification into unit, integration, protocol, header, and contract
  roles and added direct completed-evidence codec and backend-drift regressions.
- Rebind already-durable completed evidence to the current lease-bound state
  projection when restart occurs after evidence publication but before terminal
  receipt sealing; republish and reconfirm only immutable evidence without
  replaying target effects.
- Reject unknown completed-object, path-consequence, and terminal-receipt enum
  values before they can enter canonical application evidence or receipt
  identities.

Repository boundary

- Extracted the existing POSIX mechanism product into the independent
  `libpkgapply-posix` repository.
- Kept semantic application policy, evidence, restart, recovery, and abstract
  backend contracts in `libpkgapply`.
- Removed fallback subproject coupling and made the public compile closure and
  implementation ELF closure separately auditable.
- Closed the exact core ABI gate with explicit export annotations, hidden default
  visibility, and a reviewed GCC/Clang ELF manifest.
- Advanced the core to SONAME 3 and public API generation 3 because the 3.0
  incoming-package authority replaces the published 2.x admission signature
  and by-value representation; no compatibility shim is retained.

libpkgapply changelog
=====================

2.3.0 - 2026-08-02
------------------

Durable application-receipt evidence
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Added a canonical, checksummed `application_receipt` encoding for completed,
  refused, failed, partially recovered, durability-unconfirmed, and
  indeterminate application outcomes.
- Reused the existing completed-application-evidence record as the subordinate
  body of successful receipts instead of defining a second filesystem-evidence
  schema.
- Required the exact immutable installation, upgrade, or removal request when
  decoding and reconstructed every receipt through the public invariant-
  enforcing factories.
- Preserved API version 2, both SONAMEs at 2, the application-receipt identity
  schema, every existing durable application protocol, and all dependency
  floors.

Deliberate boundary
~~~~~~~~~~~~~~~~~~~

Version 2.3.0 adds no receipt store, journal traversal, target observation,
application replay, recovery actuation, installed-state projection, state
publication, discovery, repair, or package-manager command. Receipt bytes are
owner evidence; the caller still supplies the immutable request authority.

2.2.0 - 2026-08-01
------------------

Target-scoped lease validation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Added `validate_target_mutation_lease_scope()` for recovery and finalization
  paths that must prove one live lease belongs to the exact application target
  and mutation-exclusion domain without inventing a stale state projection.
- Kept `validate_target_mutation_lease()` as the stronger actuator gate and
  made it reuse the scope validation before checking the exact projection lease
  instance.
- Preserved API version 2, both SONAMEs at 2, every durable schema, the POSIX
  acquisition mechanism, and all generation-2 source/build authority floors.

Deliberate boundary
~~~~~~~~~~~~~~~~~~~

Version 2.2.0 adds no lease acquisition, installed-state read, application,
publication, reconciliation, retry, repair, or package-manager command. A
scope-valid lease alone does not authorize filesystem mutation or state
publication.

2.1.0 - 2026-08-01
------------------

Caller-owned POSIX target exclusion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Added a canonical mutation-lease acquisition value binding one exact
  application target context, mutation-exclusion domain, and mechanism-issued
  nonce to one acquisition identity.
- Added `pkgapply::posix::target_mutation_lease`, acquired from one explicit
  lock-directory descriptor with a deterministic domain-derived filename,
  final-symlink refusal, and nonblocking exclusive advisory locking.
- Kept waiting, retry, backoff, installed-state reads, target observation,
  application execution, state publication, and cleanup outside acquisition.
- Retained the coordination file after release and report a held lease as lost
  if its named lock authority is unlinked or replaced.
- Preserved API version 2, SONAME 2, all durable application schemas, and the
  generation-2 source/build authority floors.

Deliberate boundary
~~~~~~~~~~~~~~~~~~~

Version 2.1.0 provides the physical outer exclusion mechanism but does not
construct lease-bound installed-state projections, publish installed state,
assemble a transaction driver, choose retry policy, or expose package-manager
mutation commands.

2.0.0 - 2026-07-29

ABI migration to the generation-2 source/build authority closure.

- Rebuilt incoming package authority against `libpkgbuild 2.0.0` and
  `libpkgsource-plan 2.0.0`.
- Advanced `libpkgapply` and `libpkgapply-posix` to SONAME 2 because
  `incoming_package_authority` and application requests retain complete build
  results by value.
- Advanced the public API version to 2.
- Preserved application planning, precondition, payload, capture, restart,
  receipt, and completed-evidence identity domains.
- Version 2.0.0 does not publish installed state, execute package lifecycle
  programs, or choose transaction order.

1.0.0 - 2026-07-27
------------------

Native incoming authority
~~~~~~~~~~~~~~~~~~~~~~~~~

- Added `incoming_package_authority`, which admits one complete successful
  `libpkgbuild 1.0.0` result together with an independently inspected
  `libpkgimage` value.
- Verify the exact build artifact digest and every ordered payload field before
  exposing source-derived `libpkgplan` candidate control.
- Require installation and upgrade requests to retain this admitted authority;
  removal remains explicitly archive-free and has no incoming build authority.
- Reject plans whose release, candidate control, artifact, image, inspection
  receipt, manifest, or archive precondition differs from the admitted build.

Authority and ABI transition
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Advanced the application-request schema to version 2, public API version to
  1, core SONAME to 1, and POSIX backend SONAME to 1.
- Added public dependency floors for `libpkgbuild 1.0.0` and
  `libpkgsource-plan 1.0.0` while preserving `libpkgimage 0.3.0` and
  `libpkgplan 0.2.0` as exact authority boundaries.
- Retained the journaled semantic engine, recovery model, completed filesystem
  evidence, and descriptor-anchored POSIX mechanisms without moving source,
  build, planning, or installed-state authority into the backend.

Operational transition
~~~~~~~~~~~~~~~~~~~~~~

- Version 0.1.0 application requests cannot be reconstructed as version 1.0.0
  requests because they did not retain native build authority.
- Complete or recover every durable 0.1.0 attempt with the 0.1.0 libraries
  before upgrading. Journals and checkpoints are not migrated or
  reinterpreted in place.
- Callers must construct install and upgrade requests from the exact native
  build result and independently inspected image used to produce the accepted
  plan. Archive filenames and caller-invented control values are not migration
  inputs.

Deliberate boundaries
~~~~~~~~~~~~~~~~~~~~~

Version 1.0.0 does not publish installed state, execute package lifecycle
material, resolve dependencies, select package candidates, discover archives
or targets from ambient configuration, compose multi-package transactions, or
claim global filesystem and state atomicity.

0.1.0 - 2026-07-25
------------------

Authority model
~~~~~~~~~~~~~~~

- Established operation-specific immutable application requests over accepted
  `libpkgplan` installation, upgrade, and removal plans.
- Bound every attempt to one target context, caller-held mutation lease,
  lease-bound installed-state projection, backend identity, capability profile,
  and durable nonce.
- Kept package selection, policy interpretation, lifecycle execution, archive
  discovery, installed-state construction, and installed-state publication
  outside the library.

Application semantics
~~~~~~~~~~~~~~~~~~~~~

- Added exact admission checks for target, lease, state, ownership, filesystem,
  archive, image, inspection-receipt, and image-entry authority.
- Added deterministic payload, capture, rejected-object, active-effect,
  recovery, final-observation, durability, completed-evidence, and receipt
  sequencing.
- Added typed precondition refusal, physical mechanism outcomes, recovery truth,
  six-domain durability, path consequences, completed application evidence, and
  terminal receipts.
- Added durable journal and restart-checkpoint models with exact attempt
  reopening and replay classification.

POSIX backend
~~~~~~~~~~~~~

- Added FD-anchored target observation, journal, checkpoint, payload, capture,
  rejected-object, active-namespace, recovery, and completed-evidence
  mechanisms.
- Added the installed `application_posix_backend` factory and private complete
  transaction composition.
- Bound transactions to exact immutable requests and `libpkgplan 0.2.0`
  structured rejected-object provenance.
- Enforced checkpoint-before-journal publication, no-effect construction,
  descriptor anchoring, physical restart revalidation, and terminal cleanup
  only after recovery authority is no longer required.

Qualification
~~~~~~~~~~~~~

- Added public-header isolation, deterministic scripted-backend failure
  injection, semantic and restart regressions, mechanism-level POSIX tests, and
  end-to-end concrete backend composition tests.
- Added scdoc manuals and warning-strict Doxygen configuration.

Deliberate boundaries
~~~~~~~~~~~~~~~~~~~~~

Version 0.1.0 does not publish installed state, execute package lifecycle
material, discover archives or target paths from ambient configuration, solve
dependencies, compose multi-package transactions, or claim global filesystem
and state atomicity.
