libpkgapply changelog
=====================

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
