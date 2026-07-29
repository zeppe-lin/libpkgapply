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
sealed source snapshot              independently inspected artifact
          |                                      ^
          v                                      |
      libpkgbuild -------------------------------+
          | successful result + exact payload
          v
  incoming_package_authority ----> source-derived candidate control
          |                                      |
          +------------------+-------------------+
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

Incoming package authority
--------------------------

Installation and upgrade do not accept an archive, candidate-control value, or
build-provenance token independently. `incoming_package_authority::admit()`
requires one complete successful `libpkgbuild` result and one independently
inspected `libpkgimage` value. Admission proves:

```text
build request source snapshot is sealed
build result is successful and complete
sealed artifact digest == inspected archive digest
ordered build payload == ordered normalized image
candidate control == libpkgsource-plan projection of build source
```

The admitted value retains all three authorities rather than flattening them
into strings. Its identity binds the build result and request, source snapshot,
payload, artifact, artifact binding, source-derived candidate, archive digest,
normalized image, and inspection receipt.

An installation or upgrade request can be constructed only when its accepted
plan names the same release, candidate identity and control, artifact, image,
inspection receipt, artifact manifest, archive precondition, and publication
control. This is a pre-mutation structural invariant. The later admission gate
revalidates the replayed archive against both the request-bound incoming
package and the plan.

Removal has no incoming package authority. It cannot smuggle an incoming
archive precondition into the application layer.

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
12. Publish the `mutating` journal state before the first rejected-store or
    active-namespace effect.
13. Publish each rejected object from its exact sealed incoming entry or exact
    pre-mutation old-object capture, retaining the immutable rejected-record
    identity returned by the backend.
14. Synchronize the rejected-object store to the guarantee selected by the
    application execution control.
15. Execute the remaining core-derived active effect graph and synchronize the
    managed target when the selected durability contract requires it.
16. Observe the complete resulting active-path universe and compare it with the
    frozen plan consequences.
17. On contradiction or unknown result truth, retain the live transaction and
    enter the recovery branch without publishing completed evidence.
18. Construct completed evidence only after every path is observed and eligible.
19. Publish and synchronize the exact completed-evidence record.
20. Seal the terminal receipt and journal with the exact receipt and completed-
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

The POSIX private payload namespace is also descriptor-anchored. A stage
directory is named by the full application-attempt identity rather than by a
package path, archive pathname, or nonce alone. Its immutable binding record
commits to the request-bound attempt, target, backend, nonce, package-image
identity, selected entry identifiers, canonical paths, declared sizes, and
regular-content identities.

Archive replay writes only selected regular entries. Pending files are private
mode-0600 records. `end()` verifies the exact declared size and SHA-256 content
identity, synchronizes the file, publishes its stable entry name, and
synchronizes the stage directory. A seal record is published only after every
selected entry completed. The seal record is identical to the binding record,
so restart lookup cannot reinterpret one stage under another image or
selection.

A retry under the same application attempt behaves according to physical
truth. An unsealed stage may be rewritten because it is private and has not
become replay authority. A sealed stage is never rewritten: archive replay is
compared byte-for-byte with the stored payloads and `seal()` returns the same
completed staging evidence. Loading a sealed stage rechecks the binding, file
type, size, stable descriptor interval, and content digest before granting a
read-only descriptor. Incomplete stages are not returned as restart authority.

This mechanism does not create active objects, rejected objects, recovery
captures, or journal facts. The complete backend composes it with the
semantic engine and retains its full application-attempt binding.

The POSIX old-object capture namespace is a separate attempt-bound authority.
Its directory name and immutable binding commit to the full application attempt,
not a package path or backend nonce. One capture record binds the exact logical
path, rejected/recovery purposes, admitted observation, and whether exact prior-
state restoration is physically supported.

Regular objects are copied from an `O_NOFOLLOW` descriptor during one stable
metadata interval. Bytes are streamed into a mode-0600 temporary file while size
and SHA-256 are checked against admission, then the file is synchronized and
linked into its immutable stable name. Non-regular captures retain the admitted
metadata after a before/after no-follow stability check. The immutable capture
record is synchronized and published only after any regular payload is visible.
A crash may therefore leave reusable private bytes, but never a record that
claims missing capture material.

Exact recovery requires known mode, ownership, and timestamp facts. Regular
objects additionally require known size and content identity. A multiply-linked
regular object is exact only when its admitted hard-link anchor resolves to the
same inode; an unknown relation remains usable as rejected bytes but cannot
claim exact topology restoration. Sockets and unclassified object kinds are not
captured as completed authority.

Capture reload verifies attempt, path, purpose, admitted observation, record
checksum, regular file type, size, stable descriptor interval, and content
identity before granting a read-only payload descriptor. Exact republication is
idempotent and does not reread a target that may already have changed. Namespace
synchronization remains a separate backend durability operation.

The POSIX rejected-object store is a separate attempt-scoped authority. Its
immutable binding commits to the application-attempt identity, request identity,
target identity, mutation-backend identity, nonce, and exactly one accepted
operation-plan identity. Separate `incoming-v1` and `old-v1` namespaces prevent
source-class aliasing. Every immutable record additionally binds the exact
rejected-effect request, logical path, source class, completed object facts,
completeness, and provenance.

Incoming publication first resolves the exact package-image entry named by the
rejected-effect request and verifies its logical path. Directories, symbolic
links, FIFOs, and device entries are published from those image facts without
inventing a payload-stage requirement. Regular and hard-link entries require a
sealed payload set bound to the same attempt, nonce, and package-image identity.
A hard-link record preserves its logical anchor relation and copies the anchor's
verified regular bytes, so restart does not depend on private incoming staging
or the current active target.

Old publication accepts only a capture from the same attempt and logical path
whose capture request explicitly admitted rejected-object use. Regular bytes are
copied from that immutable pre-mutation capture. The store never reopens the
managed-target path and therefore cannot substitute a post-mutation pathname for
old-object authority.

Regular payloads are size- and digest-verified, synchronized, and linked into
immutable stable names before their records are published. Record identities are
domain-separated over canonical source-bound record bodies. Exact republication
verifies the existing request, source, object facts, and payload before
returning the same record and backend-evidence identities. Restart loading
revalidates the
attempt and plan bindings, record checksum, request, source class, object facts,
and any regular payload before granting a read-only descriptor. Corrupt bindings
and records become typed rejected-store failures.

Non-regular objects remain typed records; the store does not materialize them as
live filesystem nodes. Record visibility does not claim rejected-store
durability. The backend must synchronize the attempt namespace separately and
report only the guarantee actually established. This mechanism neither mutates
the active namespace nor classifies the complete application outcome.

POSIX active namespace publication
----------------------------------

The active namespace is not another application controller. The complete POSIX
transaction binds the managed target root, application attempt, optional exact
package image, sealed payload authority, admitted observations, old-object
captures, journal, and outer lease once. Its `execute_active()` implementation
then receives only the exact command derived by the semantic engine.

A mechanism result has a strict visibility meaning. `completed` means the
requested effect is visible. `conditional_retained` is valid only when
`remove_directory_if_empty` proves a non-empty directory remained unchanged.
`failed` proves the logical target unchanged. Any syscall sequence that may
have changed the logical target but cannot prove the resulting state reports
`indeterminate`; the POSIX layer does not classify application success.

Incoming non-directory objects are prepared under an exclusive attempt-bound
name in the exact destination parent. Regular bytes are copied from the sealed
payload descriptor, verified, and assigned metadata before publication.
Symbolic links, FIFOs, and permitted device nodes are likewise complete before
their parent-local name is renamed onto the final leaf. The backend never opens
the final regular path with `O_TRUNC`, and a central staging filesystem is not
used as a substitute for same-filesystem publication.

A directory activated over an existing directory preserves that inode and
unmanaged children while applying only the planned directory metadata. A type
change involving a directory uses deterministic parent-local new and displaced
names; it never recursively deletes the directory. An incoming non-directory
may replace an existing directory only after that directory is proven empty.

An incoming hard link is created with `linkat()` from its exact logical anchor,
then verified as the same regular inode before publication. It is never copied
as an unrelated regular file. Because POSIX hard links share inode metadata,
the reference backend rejects an image binding whose hard-link mode, owner,
group, or timestamp differs from its regular anchor. `libpkgimage` should make
that impossible-image invariant canonical; until then the backend preflight
must refuse it before active mutation.

`remove_observed` is non-recursive. `remove_directory_if_empty` maps a proven
non-empty result to `conditional_retained`. Unexpected absence, type change, or
race after admission is indeterminate unless the mechanism can prove that its
own command established the final state.

Recovery is selected and ordered by the core. The POSIX transaction restores
one path from deterministic workspace facts and the exact admitted prior-state
authority. Prior regular bytes come only from a verified capture, prior special
objects from exact captured facts, and prior absence is restored only by
removing an object proven to belong to the same attempt. Ambiguous workspace or
final-path state is indeterminate; incomplete capture authority never becomes a
claim of exact restoration.

Visibility and durability remain separate. Active publication records dirty
regular descriptors and affected parent directories. Synchronizing the active
namespace flushes the required content, metadata, and directory entries, and
clears no dirty state until the whole selected guarantee succeeds. A successful
`renameat()` is not promoted into global filesystem atomicity or durability.

The deterministic parent-local names are restart machinery, not canonical
application evidence. A reopened transaction inspects those names and the
logical leaf to distinguish an unexposed prepared object, a removed or replaced
object with displaced old authority, a visibly published incoming object, and
contradictory physical state. An unresolved journal intent is never blindly
issued a second time.

The reference implementation is a private, non-installed active-namespace
session rather than a second public executor. It binds image, payload,
observation, capture, target-root, and attempt authorities once. Existing
non-directory objects with recovery authority are displaced before replacement
or removal, preserving hard-link groups as physical old-object authority.
Recovery consumes deterministic workspace truth first, then exact capture
material. Missing or contradictory authority reports `indeterminate`. After a
terminal application journal is durable, the complete transaction may discard
only displaced old leaves belonging to that attempt; unresolved workspace state
is never garbage-collected as though it were committed.

The session records completed in-process effect paths for recovery. Durable
restart reconstruction of that effect prefix remains the responsibility of the
complete POSIX backend transaction, which rebuilds the private session from
the validated journal and checkpoint before calling it. The mechanism alone
does not discover attempts, select journals, or classify terminal application
success.

POSIX completed-evidence storage
--------------------------------

Completed application evidence is a terminal application record, not a restart
checkpoint and not installed-state truth. The semantic engine constructs it only
after every selected effect, required synchronization, and final observation has
completed. The POSIX backend may publish only that exact validated value; it does
not rebuild evidence from journal progress, infer missing facts, or weaken the
completed-evidence eligibility rules.

The immutable record binds the completed-evidence identity and the full evidence
body: operation kind, request, plan, attempt, target, execution control, lease-
bound state projection, journal, normalized path consequences, six-domain
durability profile, and backend evidence. Its storage encoding is versioned,
bounded, checksummed, and independently revalidated against the immutable
application request before a reopened record is accepted. A journal identity or
checkpoint reference alone is never substituted for the evidence body.

Publication uses a private mode-0600 temporary regular file, synchronizes the
complete record before exposure, and installs an identity-keyed immutable name
without replacement. Exact republication is idempotent only when the existing
bytes decode to the same completed evidence. Conflicting, truncated, corrupt,
foreign-request, or identity-inconsistent records are typed failures and are
never treated as completed publication.

Visibility and durability remain separate. `publish_completed_evidence()` may
report `completed` after the immutable record is visible and returns exactly the
recorded completed-evidence identity. The later
`synchronize(completed_evidence)` operation flushes the record and namespace
metadata and reports only the durability it established. It does not upgrade
journal, checkpoint, active-namespace, rejected-store, or installed-state
durability.

The completed-evidence store is independent of the restart-checkpoint store.
Checkpoints retain resumable mechanism progress and may contain a copy of
completed evidence needed to continue terminal sealing. The completed-evidence
store publishes the terminal proof consumed by the caller and future state-side
adapter. Neither store acquires `libpkgstate` authority, and neither may publish
installed state.

On restart, a checkpoint that says evidence publication completed is accepted
only after the complete backend verifies the same immutable record. An unresolved
publication intent is not blindly repeated; the backend first distinguishes no
record, exact visible record, and contradictory storage state. Terminal cleanup
must not remove completed evidence while a receipt or durable journal still
references it.

POSIX backend transaction composition
-------------------------------------

The concrete POSIX backend is a mechanism composition root, not another
semantic engine. Its installed factory exposes the `application_backend`
contract required by the package manager. The concrete transaction remains
private to `libpkgapply-posix`; callers cannot invoke the journal, staging,
rejected, active, recovery, or evidence mechanisms out of core-derived order.

One backend instance is configured with retained descriptors and immutable
identities for the selected target root and every storage namespace. The
configuration must reproduce the target context's observation backend,
mutation backend, capability profile, root view, active namespace, rejected
store, staging namespace, journal namespace, and exclusion domain. A
transaction never discovers a root pathname, storage path, package archive,
plan, or lease from ambient configuration.

A fresh transaction borrows the caller's exact lease instance, duplicates the
configured descriptors, and issues one unpredictable attempt nonce. Beginning
a transaction performs no observation, journal publication, payload replay,
capture, rejected publication, active mutation, synchronization, or evidence
publication. Installation and upgrade bind the exact incoming package image;
removal binds no incoming-image authority and cannot later acquire one.

The transaction owns one coherent live view of the POSIX mechanisms:

```text
target observer
journal store              restart-checkpoint store
payload store              old-object capture store
rejected-object store      active-namespace session
completed-evidence store
```

The observer, capture store, and active session must remain anchored to the
same selected target-root object. All stores remain anchored to the namespace
descriptors fixed by backend configuration. Replacing a pathname used to open
a descriptor cannot redirect a live transaction to another target or store.

Each virtual operation delegates to exactly one mechanism and retains the
returned physical fact in transaction state. `synchronize(domain)` has an
explicit routing table for all six durability domains; it does not flush an
unrelated store or infer confirmation from an earlier rename, link, or journal
write. Backend exceptions preserve whether publication or replacement may
already be visible so the core can classify uncertainty truthfully.

`publish_journal()` is also the restart publication barrier. Before a journal
snapshot that depends on new mechanism facts becomes durable current truth,
the transaction constructs the exact checkpoint for that snapshot and
publishes it immutably. The checkpoint may become durable before the journal
snapshot; such an unreferenced checkpoint is harmless. The reverse order is
forbidden because it could expose a resumable journal without its exact replay
material. A journal snapshot and checkpoint are never updated in place as one
invented cross-file atomic object.

Reopening does not allocate a new attempt. The transaction derives the original
nonce from the supplied durable journal, reports that exact journal through
`resumed_journal()`, and loads only the checkpoint keyed by that journal-record
identity. It then verifies every checkpoint claim against the corresponding
physical authority: sealed payloads, captures, rejected records, active
workspace and final-path state, synchronization facts, and completed evidence
when present. Missing or contradictory authority is a restart failure or
indeterminate physical result, never permission to repeat an unresolved active
or recovery command.

The active session is rebuilt from the admitted observation closure, exact
captures, optional incoming image, sealed payloads, and durable forward and
recovery prefixes. Completed effects are registered as already attempted;
unresolved intents are represented as indeterminate and are not reissued.
Final observation may be repeated because it is read-only. Exact idempotent
private publication and synchronization may be retried only where the core
restart contract permits it.

Transaction destruction closes descriptors and abandons only unsealed private
construction. It does not delete durable checkpoints, captures, rejected
records, completed evidence, or unresolved active workspace. Displaced old
objects are discarded only after a terminal journal is durable and the core has
made recovery unnecessary. RAII cleanup never becomes transaction resolution.

The core does not enumerate durable attempts or select a journal. The caller
supplies one validated durable journal to `resume_application()`. The complete
backend reopens exactly that attempt and loads only the checkpoint keyed by the
supplied journal snapshot.

The installed implementation is
`application_posix_backend::from_directory_fds()`. Configuration duplicates the
already-selected target and store directory descriptors. Its public surface is
only the abstract backend contract; mechanism order and mutable transaction
state remain private to `libpkgapply-posix`.

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

The reference `libpkgapply-posix` library contains FD-anchored journal,
restart-checkpoint, and completed-evidence stores; target observation; private
incoming-payload staging; attempt-bound old-object capture; immutable rejected-
object publication; active namespace mutation and recovery; and the installed
`application_posix_backend` factory. Its private transaction binds those
mechanisms to one exact request, target, borrowed lease, attempt, durability
router, and restart view without becoming another semantic engine.

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
9. Replay occurs into an attempt-bound private stage before target mutation.
10. A sealed payload stage is never reinterpreted under another attempt, image,
    or regular-entry selection.
11. Old-object evidence is captured before destructive effects.
12. The semantic core derives effect order; backends do not choose policy.
13. Every destructive effect has a durable write-ahead record.
14. A receipt records actual outcome, recovery, and durability boundaries.
15. Completed evidence exists only after complete application success.
16. Planned ownership, completed object evidence, durable installed ownership,
    and current filesystem observation remain distinct.
17. Removal requires no current candidate, source, provider, artifact, or
    archive.
18. Version 0.1.0 executes no unbound lifecycle declaration.
19. Version 2.0.0 installation and upgrade requests retain exactly one
    successful native build, independent image inspection, and source-derived
    candidate projection.
20. A valid plan from another build, image, receipt, artifact, or candidate
    universe is rejected before mutation even when package names match.
19. `libpkgapply` does not publish installed state.
20. `libpkgstate` is absent from the core dependency graph.
21. No receipt or journal record invents global filesystem/state atomicity.

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
