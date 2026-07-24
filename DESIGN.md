libpkgapply design
==================

Purpose
-------

`libpkgapply` owns package-application truth.

It answers one question:

> What effects were attempted and completed while applying this exact
> accepted package-operation plan to this exact managed target?

It does not decide what should happen. That is planning authority. It does
not decide what becomes durable installed truth. That is installed-state
authority. It does not decide how several package operations, lifecycle
actions, maintenance actions, and state publications form one complete
transaction. That is orchestration authority.

Authority graph
---------------

```text
candidate, artifact, image, installed, observation, and policy facts
                              |
                              v
                         libpkgplan
                              |
                              v
                 accepted package-operation plan
                              |
                              v
                        libpkgapply
                              |
              +---------------+---------------+
              |                               |
              v                               v
     application receipt         completed application evidence
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

Accepted plan
-------------

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

Public application facade
-------------------------

The package-manager-facing API exposes three operation-specific `apply()`
overloads. Installation and upgrade retain a caller-owned `package_archive`;
removal has no incoming archive parameter. Every overload also requires the
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

Target application context
--------------------------

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

The lifecycle-executor identity is explicitly absent in schema version 1.
Version 0.1.0 executes no package lifecycle declaration.

The context contains no installed-snapshot identity and no root pathname.
Concrete handles and locators belong to the call-scoped backend resources.
A pathname is not a target or root-view identity.

Outer mutation lease
--------------------

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

The lease establishes exclusion only among cooperating actors. Filesystem
operations must still use stable root handles, no-follow component traversal,
and final observation because unrelated processes may ignore the protocol.

Lease-bound state projection
----------------------------

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

Precondition revalidation
-------------------------

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

Incoming archive authority
--------------------------

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

Effect domains
--------------

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

Completed-object evidence
-------------------------

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

Unknown is never silently promoted to known. Inode numbers and temporary
pathnames may support backend observation but do not enter canonical
application evidence.

Application result domains
--------------------------

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

Rejected objects
----------------

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

Application sequence
--------------------

The non-virtual semantic engine owns this sequence:

1. Validate plan schema and internal authority bindings.
2. Validate target context, backend, capabilities, and outer lease.
3. Revalidate lease-bound installed-state facts.
4. Reobserve and compare every operated filesystem path exactly once.
5. Validate the exact archive replay authority when applicable.
6. Derive the payload, capture, rejected, active, observation, and durability
   effect graph without executing it.
7. Durably publish the complete `preparing` journal before capture, replay, or
   any externally visible effect.
8. Capture old objects required for rejected staging or recovery.
9. Replay required regular payloads into private incoming staging.
10. Synchronize required private staging domains and publish the `prepared`
    journal state.
11. Publish the `mutating` journal state before the first rejected-store or
    active-namespace effect.
12. Publish each rejected object from its exact sealed incoming entry or exact
    pre-mutation old-object capture, retaining the immutable rejected-record
    identity returned by the backend.
13. Synchronize the rejected-object store to the guarantee selected by the
    application execution control.
14. Execute the remaining core-derived active effect graph and synchronize the
    managed target when the selected durability contract requires it.
15. Observe the complete resulting active-path universe and compare it with the
    frozen plan consequences.
16. On contradiction or unknown result truth, retain the live transaction and
    enter the recovery branch without publishing completed evidence.
17. Construct completed evidence only after every path is observed and eligible.
18. Publish and synchronize the exact completed-evidence record.
19. Seal the terminal receipt and journal with the exact receipt and completed-
    evidence identities.

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

Operation-specific semantics
----------------------------

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

Lifecycle exclusion in version 0.1
----------------------------------

The released plan schema does not bind exact lifecycle declaration material,
phase selection, executor identity, execution policy, target execution
context, or ordering relative to application and state publication.

Version 0.1.0 therefore executes no lifecycle declaration. Supplying arbitrary
executable material beside an accepted plan would create a second controller
input not authorized by planning.

A later lifecycle surface requires an accepted plan or transaction plan that
binds the declaration, exact material, selected phase, executor, target
context, execution policy, and ordering.

Journal and crash recovery
--------------------------

A durable write-ahead journal is part of version 0.1.0.

Before each destructive effect:

1. required recovery material is made durable;
2. an effect-intent record is appended and synchronized;
3. the effect is performed; and
4. an effect-completed record is appended and synchronized.

A crash between intent and completion is not guessed to be either old or new
state. Restart handling reopens the original durable attempt under a newly
acquired outer lease and requires the backend to provide an exact replay
checkpoint bound to the durable journal snapshot.

The core classifies validated durable snapshots as forward-resumable,
recovery-resumable, terminal, or requiring external resolution. Restart
admission verifies the original request, plan, target, execution control,
backend, attempt nonce, and journal identity before any observation or replay.
The replay checkpoint retains admitted observations, private payload staging,
old-object captures, rejected records, active and recovery outcomes,
synchronization facts, backend evidence, and completed evidence when present.
Checkpoint facts are reconciled with journal intents and terminal events; a
contradiction is a backend contract violation, not a reason to guess.

Completed forward effects are reconstructed and skipped. An active or recovery
intent without a terminal event is never issued again: replay treats its
physical result as indeterminate and enters recovery or external-resolution
semantics. Unstarted forward effects may continue in frozen schedule order.
Final observation may be repeated because it is read-only. Private incoming
staging, old-object capture, and durability synchronization may be retried
under the same attempt because they do not repeat a managed-target actuator
command and remain backend-idempotent by attempt identity. Terminal journals
are never silently reopened as new attempts.

The core owns the versioned journal wire format because journal field order,
enum tags, digest domains, and identity verification are semantic protocol.
Decoding is bounded, rejects trailing or malformed data, reconstructs every
derived identity through the public model constructors, and requires the
encoded record identity to match the reconstructed snapshot. A replacement
snapshot must retain the same journal header and effect graph, preserve the
entire prior event prefix, retain any resolution identities, follow an allowed
execution-state transition, and leave receipt-bearing terminal snapshots
immutable.

The checkpoint wire format is also core-owned semantic protocol. It records
the exact journal-record identity, admitted observations, private staging and
capture outcomes, rejected and active effects, recovery effects,
synchronization facts, six-domain durability truth, aggregate backend evidence,
and completed evidence when present. A length-qualified envelope distinguishes
truncation and trailing data before a SHA-256 body checksum detects same-length
corruption. Decoding is bounded and requires the
exact journal snapshot plus immutable application request; plan-derived path
roles, ownership transitions, and intended outcomes are reconstructed from that
request rather than accepted as a second controller input.

The POSIX storage layer remains deliberately byte-oriented. Journal storage
derives one safe filename from the stable journal identity, opens the store as a
retained directory descriptor, writes a mode-0600 temporary regular file,
synchronizes the file, atomically renames it over the prior snapshot, and
synchronizes the directory. Failure after rename is reported separately as a
visible replacement whose directory durability is unconfirmed. Loads use
`openat()` without following the final symlink, require a bounded regular file,
decode the complete stream, and verify that filename and journal content name
the same journal.

Checkpoint storage derives a separate safe filename from the exact journal-
record identity. Each snapshot is immutable: publication writes and synchronizes
a mode-0600 temporary file, links it into the directory without replacement,
removes the temporary name, and synchronizes the directory. Exact republication
is accepted only when the existing bytes are identical; conflicting bytes under
the same journal-record identity are rejected. Loads verify the stored checksum,
exact journal binding, and typed request binding before returning replay facts.

The core does not discover application attempts. The complete backend still
selects which validated journal snapshot and checkpoint belong to a durable
attempt, then supplies both to `resume_application()`.

State integration
-----------------

`libpkgapply` produces completed application evidence. It does not construct:

```text
pkgstate::installed_control
pkgstate::installed_package
pkgstate::package_state_delta
pkgstate::state_publication_request
pkgstate::state_publication_receipt
```

A future destination-owned `libpkgstate-apply` adapter should consume accepted
plan, completed application evidence, complete installed-control material,
prior state snapshot, and target binding. It then constructs state-owned
objects and the publication request.

The `libpkgapply` repository may contain a non-installed integration test for
this seam. `libpkgstate` must remain absent from the core headers, library, and
pkg-config dependency closure.

Core and backend split
----------------------

`libpkgapply` contains:

* immutable public model values;
* canonical identities;
* precondition and result semantics;
* journal semantic model;
* non-virtual application sequencing; and
* constrained backend interfaces.

The reference `libpkgapply-posix` library is built in mechanism-sized
tranches. It currently contains FD-anchored journal and immutable restart-
checkpoint stores. Its complete boundary will contain:

* target-root and lease interoperability;
* FD-anchored observation;
* private staging;
* journal and restart-checkpoint storage;
* active namespace mutation;
* rejected-object storage;
* durability synchronization; and
* conservative restart recovery.

The core depends publicly on `libpkgplan` and `libpkgimage`, and privately on
its identity implementation. It has no direct archive-decoder dependency and
no installed-state dependency.

Concurrency and errors
----------------------

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

Hard invariants
---------------

1. Application consumes an accepted plan and never reparses policy.
2. The three released plan kinds remain distinct public request types.
3. The target context must equal the target identity cited by the plan.
4. The caller holds the outer mutation lease through state resolution.
5. Snapshot, ownership, owner vectors, and path facts are revalidated under
   that lease before mutation.
6. Additional fresh knowledge does not make a precondition stale; every known
   accepted fact must still match.
7. Installation and upgrade replay only from the exact retained archive
   authority cited by the plan.
8. Archive pathname and filename are never replay or package authority.
9. Replay occurs into private staging before target mutation.
10. Old-object evidence is captured before destructive effects.
11. The semantic core derives effect order; backends do not choose policy.
12. Every destructive effect has a durable write-ahead record.
13. A receipt records actual outcome, recovery, and durability boundaries.
14. Completed evidence exists only after complete application success.
15. Planned ownership, completed object evidence, durable installed ownership,
    and current filesystem observation remain distinct.
16. Removal requires no current candidate, source, provider, artifact, or
    archive.
17. Version 0.1.0 executes no unbound lifecycle declaration.
18. `libpkgapply` does not publish installed state.
19. `libpkgstate` is absent from the core dependency graph.
20. No receipt or journal record invents global filesystem/state atomicity.

## FD-anchored target observation

`libpkgapply-posix` observes managed target objects relative to a retained root
directory descriptor. Parent components are opened one at a time with
`openat(2)`, `O_DIRECTORY`, and `O_NOFOLLOW`; a symbolic-link parent is an
observation error rather than an alternate route through the host namespace.
The leaf is inspected with no-follow metadata operations, so a leaf symbolic
link is reported as a symbolic link and is never traversed.

Regular content identities are SHA-256 digests of bytes read from an opened
regular-file descriptor. Metadata is sampled before and after the read. A
replacement or concurrent modification yields an unknown observation instead
of evidence assembled from different objects. Hard-link relations are claimed
only when the caller supplies an expected logical anchor and both paths are
observed as the same regular inode. The observer does not infer package
semantics from arbitrary inode aliases.

This layer is observation mechanism only. It does not acquire mutation leases,
execute active effects, capture recovery objects, or publish durable records.
