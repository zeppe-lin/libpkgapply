# libpkgapply design

## Purpose

`libpkgapply` owns package-application truth.

It answers one question:

> What effects were attempted and completed while applying this exact
> accepted package-operation plan to this exact managed target?

It does not decide what should happen. That is planning authority. It does
not decide what becomes durable installed truth. That is installed-state
authority. It does not decide how several package operations, lifecycle
actions, maintenance actions, and state publications form one complete
transaction. That is orchestration authority.

## Authority graph

```text
successful build result        independently inspected image
          |                                  |
          +----------------+-----------------+
                           v
                  libpkgbuild-image
             admitted build/image authority
                           |
                           v
                  libpkgbuild-plan
       source candidate + planner artifact projection
                           |
                           v
              incoming_package_authority
                           |
                    accepted libpkgplan
                           |
                           v
                      libpkgapply
                           |
             +-------------+-------------+
             |                           |
             v                           v
    application receipt      completed application evidence
                                         |
                                         v
                           libpkgstate-side projection
                                         |
                                         v
                             state-publication request
                                         |
                                         v
                                    libpkgstate
```

The package manager remains the composition root. It acquires the outer
target mutation lease, supplies immutable authorities, retains the lease
through installed-state publication and recovery choice, and records the
complete transaction result.

## Incoming package authority

Installation and upgrade do not accept an archive, candidate-control value,
build result, or inspected image independently. `incoming_package_authority`
imports one complete `libpkgbuild-plan::artifact_projection` into application
vocabulary.

The projection already retains:

```text
libpkgbuild-image admission of one successful build and matching image
libpkgsource-plan projection of the sealed build subject
libpkgplan artifact fact derived from those same authorities
```

`libpkgapply` does not repeat payload/image equality or source projection. Its
incoming identity binds the complete upstream projection, source, candidate,
artifact, manifest, and release authorities.

An installation or upgrade request can be constructed only when its accepted
plan names the same release, candidate identity and control, artifact,
artifact manifest, archive, image, inspection receipt, archive precondition,
and publication control. This is a pre-mutation structural invariant. The
later admission gate revalidates the replayed archive against both the
request-bound incoming package and the accepted plan.

Removal has no incoming package authority. It cannot smuggle an incoming
archive precondition into the application layer.

## Accepted plan

The public application API consumes the released operation-specific
`libpkgplan` values directly:

```text
installation_plan
upgrade_plan
removal_plan
```

There is no application-owned editable copy of plan decisions. In
particular, callers and backends cannot replace:

* operation kind;
* operation-plan identity;
* target, snapshot, ownership, observation, archive, image, or receipt
  bindings;
* path roles;
* active-object outcomes;
* rejected-object outcomes;
* ownership transitions; or
* installed-state publication intent.

The plan's canonical path order is identity and presentation order. It is
not assumed to be payload replay order or safe filesystem execution order.
The application core derives the effect dependency graph and uses canonical
path order only as a deterministic tie-breaker.

## Public application facade

The package-manager-facing API exposes three operation-specific `apply()`
overloads. Installation and upgrade requests retain admitted native build and
image authority while the call borrows the exact replayable
`package_archive`; removal has neither incoming authority nor an archive
parameter. Every overload also requires the
exact lease-bound state projection, the caller-held outer mutation lease, and
one selected backend.

One call owns one backend transaction from fresh admission through terminal
resolution. Typed active-effect and final-observation interruptions do not
escape as private engine sessions. The facade applies the request's recovery
control while the same transaction, journal, captures, and staged authorities
remain live, then returns one immutable terminal receipt.

Static authority errors, invalid canonical records, backend-contract
violations, allocation failure, and early mechanism exceptions remain
exceptions. The facade does not fabricate a receipt when physical truth cannot
be established. It also does not publish installed state; a successful receipt
and completed evidence remain inputs to `libpkgstate`.

## Target application context

One immutable application target context binds at least:

```text
target-system-context identity
managed-target identity
root-view identity
observation-backend identity
mutation-backend identity
mutation-exclusion-domain identity
active-object namespace identity
rejected-object-store identity
private-staging namespace identity
application-journal namespace identity
execution-capability-profile identity
optional lifecycle-executor identity
```

The optional lifecycle-executor identity is target-context authority for
orchestration that coordinates lifecycle work around application. It does not
grant `libpkgapply` authority to execute lifecycle declarations itself.

The context contains no installed-snapshot identity and no root pathname.
Concrete handles and locators belong to the call-scoped backend resources.
A pathname is not a target or root-view identity.

## Outer mutation lease

`libpkgapply` requires a caller-held target mutation lease. It does not own
the outer lease lifetime.

The required sequence is:

```text
pkgman acquires outer target lease
    -> state is reread under that lease
    -> libpkgapply validates and applies the plan
    -> libpkgstate publication is attempted
    -> pkgman selects finalization or recovery
    -> outer target lease is released
```

Releasing the outer lease when `execute()` returns would create an
uncontrolled interval between filesystem effects and state publication. The
actuator therefore borrows the lease and returns while the caller still owns
it.

A state backend may acquire its own narrower publication lock while the
outer lease is held. Lock order is outer target lease first, state
publication lock second.

Concrete lease acquisition is mechanism-provider authority. The core accepts
only the immutable `target_mutation_lease` capability and validates its target,
exclusion domain, acquisition identity, and relationship to the supplied state
projection. It does not select a lock location, open a coordination object,
wait, retry, or define host locking policy.

The lease establishes exclusion only among cooperating actors. Backends must
still preserve target anchoring and report final observation because unrelated
processes may ignore the protocol.

Lease validation is capability-specific.
`validate_target_mutation_lease_scope()` proves that a live acquisition belongs
to the exact target context and exclusion domain. It is sufficient only for a
recovery or finalization step that observes canonical target state and performs
no application or publication. `validate_target_mutation_lease()` is the
stronger actuator gate: it first validates that scope, then proves the supplied
installed-state projection was established under the same acquisition instance.
A caller must not manufacture the old projection after canonical publication
has already advanced to a new state epoch merely to satisfy an actuator-shaped
interface.

## Lease-bound state projection

The core library has no `libpkgstate` dependency. A caller supplies an
immutable projection established under the outer lease:

```text
lease-instance identity
installed-state snapshot identity
ownership-inventory identity
exact owner vector for every operated path
projection completeness
projection-evidence identity
```

This value is not installed truth owned by `libpkgapply`. It is an explicit
projection from the selected installed-state authority for application-time
revalidation.

## Precondition revalidation

Before target-domain mutation, application verifies:

* target context identity;
* current installed snapshot identity;
* current ownership-inventory identity;
* exact owner vectors for every operated path;
* explicit present or absent path state; and
* every object fact known by the accepted path precondition.

Fresh application observations may establish more facts than the planning
observation. Additional established facts do not make the plan stale. Every
fact known by the plan must, however, still be known and equal.

A mismatch returns a typed no-mutation refusal. Application does not
replan, weaken policy, or improvise another outcome.

## Incoming archive authority

Installation and upgrade receive one stable replayable
`pkgimage::package_archive` authority. Application verifies it against the
plan's exact:

```text
complete archive digest
package-image identity
archive-inspection-receipt identity
```

A pathname is not an admissible replay authority. Application does not
reopen an archive or infer package identity from its name.

Only regular payloads are replayed. The selected payload closure includes
regular entries required by:

* active incoming objects;
* rejected incoming objects; and
* hard-link anchors required by either outcome.

Replay occurs into private staging. `libpkgimage` replay may partially
deliver bytes before failure and is not itself a transaction.

## Effect domains

Each operated path keeps independent consequences:

```text
active target object
rejected object
ownership transition
```

Retaining bytes, staging bytes, and retaining ownership are different facts.
Application executes exactly the plan's selected consequences and reports
the actual completed result of each dimension.

The first public model distinguishes at least:

```text
regular file
directory
symbolic link
hard-link relation
FIFO
character device
block device
socket
other or unknown object
```

Incoming sockets cannot occur in the `libpkgimage 0.3.0` archive model. An
observed socket may be retained or removed when the accepted plan says so.
Application never invents unsupported restoration semantics.

## Completed-object evidence

Completed object facts remain richer than the current installed-state
ownership representation. They may retain:

* exact object kind;
* mode, owner, and group;
* regular size and content identity;
* symbolic-link target;
* device number;
* hard-link relation;
* selected timestamp semantics when explicitly supported;
* completeness; and
* provenance.

Fields distinguish:

```text
known(value)
unknown
not applicable
```

For regular objects, the hard-link relation is an optional topology qualifier.
`unknown` means that no peer anchor relation is asserted; it does not by itself
make an otherwise publication-complete regular object partial. Completeness is
therefore about the facts required to publish that object, not about forcing
every applicable qualifier to `known(value)`.

Unknown is never silently promoted to known. Inode numbers and temporary
pathnames may support backend observation but do not enter canonical
application evidence.

## Application result domains

Every trustworthy attempt produces an immutable application receipt.
Completed application evidence exists only when all selected effects and
required final observations completed and required application durability
was established.

Receipt outcomes include:

```text
precondition refused
failed before target mutation
completed
failed and fully recovered
failed with partial effects
effects visible with durability unconfirmed
indeterminate
```

A failed receipt and completed evidence occupy different identity domains.
No failure value can be parsed or used as completed application evidence.

Durability is recorded independently for:

```text
journal
incoming staging
recovery staging
active namespace
rejected-object store
completed evidence record
```

The receipt records only guarantees actually established by the backend.
It never upgrades rename, synchronization, signal handling, or process
cleanup into unsupported global atomicity.

## Durable application-receipt encoding

The core owns one versioned, length-qualified, SHA-256-protected encoding for a
validated terminal `application_receipt`. Decode requires the complete immutable
installation, upgrade, or removal request. Request, operation kind, receipt
identity, and every reconstructed factory result must agree with that authority.

A completed receipt embeds the existing canonical completed-application-
evidence encoding, plus the receipt recovery state and outer backend evidence.
It does not repeat the path, durability, attempt, projection, or journal body.
A non-completed receipt retains its exact attempt, state projection, outcome,
recovery state, durability profile, partial path consequences, optional journal,
and backend evidence. Canonical re-encoding must reproduce the exact input.

The decoder performs no journal lookup, target observation, filesystem access,
application replay, recovery, evidence publication, or installed-state
publication. A receipt codec is evidence admission, not an actuator or store.

## Rejected objects

Canonical rejected storage is attempt-scoped and distinguishes incoming from
old objects:

```text
<store>/<attempt-identity>/incoming/<encoded-logical-path>
<store>/<attempt-identity>/old/<encoded-logical-path>
```

Records preserve logical path, source class, plan and attempt identities,
object facts, payload or relation data, completeness, and provenance.

`stage_old` captures the object before destructive mutation. A later copy of
the pathname cannot be reported as old-object evidence.

The canonical representation need not materialize dangerous special objects
as live nodes. Compatibility projection into the historical mirrored
rejected tree remains a separate adapter concern.

## Application sequence

The non-virtual semantic engine owns this sequence:

1. Validate plan schema and internal authority bindings.
2. Validate target context, backend, capabilities, and outer lease.
3. Revalidate lease-bound installed-state facts.
4. Reobserve and compare every operated filesystem path exactly once.
5. Validate the exact archive replay authority when applicable.
6. Derive the payload, capture, rejected, active, observation, and durability
   effect graph without executing it.
7. Publish one immutable application declaration and initial bounded cursor
   through the separate journal store before any mechanism effect.
8. Before each mechanism invocation, append its immutable intent step and
   durably advance the bounded cursor.
9. Capture old objects required for rejected staging or recovery.
10. Replay required regular payloads into private incoming staging.
11. After each mechanism result, append exactly one typed terminal step and
    durably advance the cursor; state-only transitions are also single steps.
12. Synchronize required private staging domains and append the `prepared` state.
13. Append the `mutating` state before the first rejected-store or active-
    namespace effect.
14. Publish each rejected object from its exact sealed incoming entry or exact
    pre-mutation old-object capture, retaining the immutable rejected-record
    identity returned by the backend.
15. Synchronize the rejected-object store to the guarantee selected by the
    application execution control.
16. Execute the remaining core-derived active effect graph and synchronize the
    managed target when the selected durability contract requires it.
17. Observe the complete resulting active-path universe and compare it with the
    frozen plan consequences.
18. On contradiction or unknown result truth, retain the live transaction and
    enter the recovery branch without publishing completed evidence.
19. Construct completed evidence only after every path is observed and eligible.
20. Publish and synchronize the exact completed-evidence record.
21. Append one terminal receipt step carrying the exact receipt and completed-
    evidence identities. Journal durability is established by successful store
    commits, not by a mutation-backend synchronization effect.

The target-mutation boundary refers to the managed active-object namespace.
The rejected-object store is an independent application-effect domain. A
receipt that failed before active target mutation may therefore report an
attempted rejected consequence while every active consequence remains
`not_attempted`. Its recovery state describes the managed active target;
rejected-store outcome and durability remain explicit in their own fields.

A completed rejected publication returns an immutable rejected-object-record
identity. Failed or indeterminate publication returns no completed record.
`visibility_only` and `journal_and_recovery` retain that established visibility
without inventing rejected-store durability. `all_application_domains` requires
a separate rejected-store synchronization and accepts only confirmed durability.
Backend implementations report mechanism outcomes. They do not skip
validation, reinterpret policy, select different paths, manufacture
ownership, or classify semantic success.

A completed-evidence record is a backend authority, not an in-memory assertion.
The backend must return the exact identity it published, and the engine accepts
that record for installed-state publication only after completed-evidence
durability is confirmed. Failed or indeterminate evidence publication leaves
the already observed target truth explicit but every path publication-
ineligible.

## Operation-specific semantics

### Installation

Installation has no old-package historical authority. It may activate
incoming objects, retain compatible observed objects, stage incoming rejected
objects, leave paths absent, and establish incoming ownership consequences.
Structural necessity alone never grants ownership.

### Upgrade

Upgrade keeps old installed authority and incoming artifact authority
separate. Old objects are captured before replacement or removal. Obsolete
paths, incoming replacement, shared retention, rejected staging, and
ownership transfer follow the accepted plan exactly.

### Removal

Removal requires no candidate, provider, source, artifact, archive, or image.
It executes from the accepted removal plan and current target authorities.
Conditional directory cleanup is non-recursive: a non-empty directory remains
and is reported as the completed conditional outcome.

## Lifecycle authority remains outside application

`libpkgplan` may retain exact lifecycle declarations for downstream
orchestration, and the target context may retain the identity of the selected
lifecycle executor. Neither fact turns lifecycle execution into an application
effect. `libpkgapply` applies the accepted filesystem operation plan only.

Lifecycle phase selection, execution ordering, restart policy, and invocation
belong to the transaction/controller layer that composes application with the
selected lifecycle executor. Supplying executable material directly to
`libpkgapply` would create a second unplanned controller input and is therefore
outside this boundary.

## Journal and crash recovery

The durable semantic history is the append-only declaration/step/cursor protocol
described below. `application_backend_transaction` does not persist journal
history and cannot synchronize journal durability. A controller supplies one
separate `application_journal_store`; a mechanism product may implement both
objects, but code-location coincidence grants no shared authority.

Before a mechanism effect, libpkgapply validates the next semantic step without
advancing its in-memory head, publishes the immutable intent step, and
compare-and-publishes the bounded cursor. Only then may the physical mechanism
run. After the mechanism returns, its typed result/evidence is encoded into one
terminal step, that step and cursor are committed, and only then does the live
engine advance. Store failure therefore cannot make a non-durable transition
look committed to a reusable engine session.

A restart is addressed by the immutable declaration identity. Libpkgapply loads
the declaration and bounded cursor, reads every committed step by exact sequence,
validates declaration, sequence, predecessor, lifecycle and effect progress, and
probes exactly `cursor.step_count()` for the one crash case where a successor
step became durable before the cursor advance. A valid orphan may advance the
head by exact compare-and-publish; conflicting, skipped, missing, or malformed
history fails closed. Directory enumeration and current-target observation are
not history-recovery mechanisms.

The declaration retains the complete deterministic effect graph and the exact
owner-authored replay seed admitted before mutation. Terminal steps retain typed
replay facts for mechanism results. Restart therefore recovers historical
semantic authority from owner-authored bytes, not from present filesystem state
or a provider reconstruction.

The current generation still asks a reopened backend transaction for an
`application_restart_checkpoint` as a temporary subordinate bridge to existing
provider evidence: private staging/capture outcomes, rejected and active results,
recovery results, physical synchronization facts, backend evidence, and completed
evidence. The checkpoint no longer supplies original admission observations, no
longer owns semantic progress, and its journal-synchronization residue is ignored.
The core first rehydrates the owner journal and then reconciles subordinate
mechanism facts against it. Contradiction is a backend contract violation.

A complete `application_journal_record` is still materialized transiently at
that pre-release backend-reopen/checkpoint seam because the old checkpoint API
uses the record identity. The projection is derived in memory, never persisted,
and never used as a second historical spine. The checkpoint API itself is the
next provider-generation migration seam; no final generation-4 release should
retain a durable checkpoint store as semantic co-authority.

Completed forward effects are skipped. An active or recovery intent without a
terminal event is never issued again: replay treats its physical result as
indeterminate and enters recovery or external-resolution semantics. Unstarted
forward effects may continue in frozen schedule order. Final observation may be
repeated because it is read-only. Private staging/capture and physical
synchronization may be retried only under their existing attempt-scoped
idempotence contracts.

A crash can occur after completed evidence is durable but before the terminal
receipt step. Historical projection and completed evidence remain immutable
proof. Restart under a newly acquired lease validates a new current projection
without rewriting historical admission. Equivalent completed application truth
may be republished against that current projection and reconfirmed before the
terminal receipt is appended; active or rejected effects are not repeated.

## Reference mechanism separation

Concrete POSIX observation, storage, mutation, and recovery mechanics are
owned by the independent `libpkgapply-posix` repository. The core specifies
only the abstract backend contracts, ordering, evidence, and recovery
obligations those mechanisms must satisfy.

## State integration

`libpkgapply` produces completed application evidence. It does not construct:

```text
pkgstate::installed_control
pkgstate::installed_package
pkgstate::package_state_delta
pkgstate::state_publication_request
pkgstate::state_publication_receipt
```

The independent, destination-owned `libpkgstate-apply` adapter consumes the
exact operation request, completed application evidence, lease-bound state
projection, expected state snapshot, and operation-specific state material. It
constructs state-owned records and one compare-and-publish request without
moving publication authority into the application core.

Qualification of that translation belongs to `libpkgstate-apply`.
`libpkgstate` remains absent from the core headers, library, tests, and
pkg-config dependency closure.

## Core and backend split

`libpkgapply` owns semantic application, not host mutation mechanics. Every
target observation and effect crosses the abstract `application_backend` and
`application_backend_transaction` interfaces. A mechanism provider owns target
access, storage, synchronization, and system-call failures.

`libpkgapply` contains:

* immutable public model values;
* canonical identities;
* precondition and result semantics;
* journal semantic model;
* non-virtual application sequencing; and
* constrained backend interfaces.

The reference `libpkgapply-posix` library contains FD-anchored journal,
restart-checkpoint, and completed-evidence stores; target observation; private
incoming-payload staging; attempt-bound old-object capture; immutable rejected-
object publication; active namespace mutation and recovery; and the installed
`application_posix_backend` factory. Its private transaction binds those
mechanisms to one exact request, target, borrowed lease, attempt, durability
router, and restart view without becoming another semantic engine.

The core public model depends on `libpkgbuild-plan` and `libpkgplan`. Its
cryptographic provider is private. Build/image admission, source projection,
and their transitive authorities remain upstream implementation needs exposed
through the opaque planner projection; they are not public pkg-config edges.
The core has no direct archive-decoder, POSIX mechanism, resolver, catalog, or
installed-state dependency. POSIX headers, filesystem APIs, lock-file
conventions, backend storage layouts, and `libpkgstate` mechanism dependencies
are forbidden from the core. `libpkgapply-posix` depends inward on this semantic
core; the core never depends outward on its reference mechanism provider.

### Backend authority

The immutable `application_target_context` is the admitted backend authority for
one request. A provider is revalidated when its transaction is accepted, and
transaction mutation identity, observation identity, and capability evidence
must equal the identities and capability profile already sealed into that
context. Restart journal backend identity is compared to the same request-bound
target authority, never to a fresh provider callback. A provider therefore
cannot change the meaning of an admitted request by reporting different
identity or capability evidence later in the call.

## Concurrency and errors

Immutable public values support concurrent read access.

One application transaction is single-threaded unless a concrete backend
explicitly documents stronger behavior. The abstract `package_archive`
interface does not promise concurrent replay.

Expected semantic outcomes are values, including stale refusal, unsupported
capability, partial effects, durability uncertainty, and indeterminate state.
Exceptions remain appropriate for invalid construction, corrupt canonical
records, backend-contract violations, allocation failure, or an early I/O
failure that prevents truthful receipt construction.

Destructors are non-throwing and release resources. RAII cleanup is not crash
recovery.

## Hard invariants

1. Application consumes an accepted plan and never reparses policy.
2. Installation, upgrade, and removal remain distinct public request types.
3. The target context equals the target identity cited by the accepted plan.
4. The caller holds the outer mutation lease through application, state
   resolution, and recovery choice.
5. Snapshot, ownership, owner vectors, and accepted path facts are revalidated
   under that lease before mutation.
6. Additional fresh knowledge does not make a precondition stale; every known
   accepted fact must still match.
7. Installation and upgrade retain exactly one successful native build,
   independent image inspection, source-derived candidate projection, and
   replayable archive authority.
8. A plan from another build, image, receipt, artifact, candidate, target, or
   state universe is rejected before mutation even when package names match.
9. Archive pathname and filename are never replay or package authority.
10. Replay occurs into an attempt-bound private stage before target mutation.
11. A sealed payload stage is never reinterpreted under another attempt, image,
    or regular-entry selection.
12. Old-object evidence is captured before destructive effects.
13. The semantic core derives effect order; backends implement mechanisms and
    do not choose policy.
14. Every destructive effect has a durable write-ahead record.
15. A receipt records actual outcome, recovery, and durability boundaries.
16. Completed evidence exists only after complete application success.
17. Planned ownership, completed object evidence, durable installed ownership,
    and current filesystem observation remain distinct.
18. Removal requires no current candidate, source, provider, artifact, image,
    inspection receipt, or archive.
19. Lifecycle declarations are not application effects; orchestration owns
    their executor, phase ordering, and restart policy even when declarations
    and executor identity are already bound upstream.
20. `libpkgapply` does not publish installed state.
21. `libpkgstate`, `libpkgapply-posix`, and host mechanism APIs are absent from
    the core dependency graph.
22. No receipt or journal record invents global filesystem/state atomicity.

## Append-only application history authority

Application history is a write-ahead protocol, not a sequence of complete
snapshots. The durable shape is therefore split into exactly three authorities:

```text
application_journal_declaration   written once
              |
              v
step 0 -> step 1 -> ... -> step N immutable append-only history
                              |
                              v
                    application_journal_cursor
                         bounded mutable head
```

The declaration owns the fixed journal header and complete deterministic effect
graph once. Each step owns only the newly-created historical transition. Replay
seed/fact bytes are semantic libpkgapply material: a mechanism store retains
them byte-for-byte but does not decode them or acquire restart vocabulary. The
cursor is a bounded locator suitable for controller retention; it never embeds
the effect graph or prior event history.

A physical journal store publishes declarations and steps immutably, advances
the cursor by exact compare-and-publish authority, and loads steps by exact
sequence. Directory enumeration and current target inspection are not history
recovery mechanisms. If a step is durable while the cursor update is missing,
restart may probe only the exact next sequence named by the cursor and validate
its declaration and predecessor before advancing the head.

`libpkgapply` rehydrates retained history itself. It loads the declaration,
reads every committed step by exact sequence, validates event progress through
an effect-identity index, and then probes exactly `cursor.step_count()` once for
a crash-orphaned successor. A missing committed step, a cursor that disagrees
with the predecessor chain, or a malformed orphan fails closed. Rehydration
never enumerates storage and never observes the managed target. The complete
`application_journal_record` may still be materialized transiently while the
pre-release engine is being migrated, but that projection is in-memory derived
state and is not durable authority.

The legacy complete-snapshot journal and restart-checkpoint persistence path is
not a compatibility target. During the pre-release migration it must be removed,
not wrapped behind an adapter. No release may ship both durable histories as
co-equal authority.
