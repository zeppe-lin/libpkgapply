# Maintaining libpkgapply

Before tagging:

1. confirm no POSIX implementation, system header, or backend storage layout
   remains in the core repository;
2. build the exact published dependency chain in GCC and Clang shared/static,
   release, and ASan/UBSan configurations;
3. run all unit, integration, protocol, header, contract, and installed-
   consumer qualification;
4. inspect pkg-config against the public semantic owners and `DT_NEEDED` against
   the reviewed implementation dependency closure plus private `libcrypto`;
5. build the shared core with GCC and Clang and require its exact dynamic
   symbol set to match `abi/libpkgapply.exports`; review any manifest change
   against the SONAME/API policy in `docs/abi.md`;
6. lint manuals and strict Doxygen, and stage-install all documentation;
7. audit integration against the corresponding `libpkgapply-posix` and
   `libpkgstate-apply` release candidates; require durable application journals
   to retain the exact admitted state-projection body and forbid recovery code
   from reconstructing that historical projection from a current state read;
8. replay the mailbox independently and compare Git trees.

The current ABI gate is closed only while the reviewed manifest, SONAME 4, and
API generation 4 remain exact in every release candidate.

## Append-only journal migration gate

The application journal is admitted for release only when there is one durable
historical spine. `application_journal_declaration` may scale with the fixed
effect graph once; each `application_journal_step` must scale only with the new
fact; `application_journal_cursor` must remain bounded. A backend or controller
must not retain a second complete restart snapshot, rewrite the event prefix on
every effect, or discover missing history by directory scan or current target
inspection.

Retained append-only history must be rehydrated by exact sequence. The loader
may probe only the one sequence immediately after the committed cursor to
resolve the step-durable/head-stale crash case. Directory enumeration, current
target observation, or per-step rescanning of the effect graph are release
blockers.

Generation 4 must retain exactly one semantic restart authority: the append-only
owner journal. `application_restart_view` is a transient projection of that
history and must never acquire a durable codec or store. Backend providers may
retain physical staging/capture/rejected/completed objects only as subordinate
evidence revalidated against that view. Reintroducing a provider-authored
restart checkpoint is a release blocker.
