% LIBPKGAPPLY(3) libpkgapply | Version 4.0.1

# NAME

libpkgapply - apply one accepted package-operation plan

# SYNOPSIS

```
#include <libpkgapply/libpkgapply.h>

pkgapply::application_receipt
pkgapply::apply(
    const pkgapply::installation_application_request& request,
    const pkgapply::lease_bound_state_projection& state,
    pkgapply::target_mutation_lease& lease,
    pkgapply::application_backend& backend,
    pkgapply::application_journal_store& journal_store,
    const pkgimage::package_archive& archive);
```

Equivalent overloads accept an upgrade request with an archive or a removal
request without archive authority.

# DESCRIPTION

**libpkgapply** is a C++17 library for applying one immutable, accepted
**libpkgplan** installation, upgrade, or removal plan to one managed target.
Installation and upgrade requests retain one complete
**libpkgbuild-plan** artifact projection. That upstream value already retains a
**libpkgbuild-image** admission proving agreement between the successful build
and independently inspected image, plus the source-derived candidate and
planner artifact facts. The library binds this incoming authority to the
accepted plan, then revalidates the target, lease, installed-state projection,
filesystem observations, and replay archive before mutation.

The semantic engine owns mechanism order. It stages selected regular payloads,
captures old objects needed for rejected publication or recovery, publishes
rejected objects before destructive active effects, appends owner-authored
journal steps through a separate durable journal store, observes the final
target, publishes completed application evidence, and returns one immutable
receipt.

The library does not select packages, parse package-manager configuration,
reinterpret planner policy, discover archives by pathname, execute lifecycle
declarations, construct installed-state objects, or publish installed state.

# APPLICATION REQUESTS

**installation_application_request**, **upgrade_application_request**, and
**removal_application_request** are distinct immutable types. Each retains the
exact accepted operation plan, application target context, and execution
control. The closed **package_application_request** envelope is used only at
backend composition boundaries; it does not erase operation-specific
semantics.

Installation and upgrade requests require an
**incoming_package_authority**. It imports one complete planner artifact
projection without repeating upstream build/image or source-projection logic.
Request construction proves that the accepted plan names the same candidate
control, release, artifact, manifest, archive, image, inspection receipt, and
archive precondition.

The complete call also borrows the exact replayable package archive. Removal
has no incoming build, archive, image, candidate, artifact, or current source
authority.

# TARGET AND LEASE

**application_target_context** binds planner target identity to the managed
target, root view, observation backend, mutation backend, exclusion domain,
active namespace, rejected store, staging namespace, journal namespace, and
capability profile.

The caller acquires one **target_mutation_lease** and retains it through
application and later installed-state resolution. A
**mutation_lease_acquisition** canonically binds one mechanism-issued nonce to the
exact application target context and mutation-exclusion domain. The supplied
POSIX implementation acquires that authority from an explicit lock-directory
descriptor without choosing wait or retry policy. **lease_bound_state_projection**
binds the selected installed snapshot, ownership inventory, path-owner closure,
and projection evidence to that exact lease instance.

**validate_target_mutation_lease_scope()** validates only that one live lease
belongs to the exact target context and mutation-exclusion domain. It is for
observation-only recovery and finalization paths where canonical state may have
already advanced and no truthful old-state projection exists.
**validate_target_mutation_lease()** additionally validates that the supplied
state projection belongs to the same acquisition and remains the required gate
before application or state-publication actuation.

# BACKEND CONTRACT

**application_backend** opens one **application_backend_transaction**. A fresh
transaction supplies one unpredictable attempt nonce and performs no effect
merely by being constructed. A resumed transaction retains the original attempt nonce and is reopened only
after libpkgapply has rehydrated the exact owner journal history.

The transaction exposes constrained operations for observation, payload
staging, old-object capture, rejected publication, active effects, recovery,
physical-domain durability synchronization and completed-evidence publication.
It does not publish or reconstruct semantic journal history. Backend reopen
receives an owner-derived **application_restart_view** only to revalidate
subordinate physical evidence.
The core invokes those operations in semantic order; a backend does not choose
policy.

A separate **application_journal_store** persists the immutable attempt
declaration, append-only semantic steps, and bounded journal cursor. Successful
store commits establish journal durability directly; the mutation transaction
does not synchronize the journal domain. Restart addresses retained history by
the exact store plus immutable declaration identity and libpkgapply rehydrates
the committed step chain before reopening the mutation backend.

Controllers may call **rehydrate_application_journal()** with that same store and
declaration identity when they need a validated in-memory record for pure
restart classification before choosing whether to resume. The owner performs the
same exact-sequence validation and one-successor orphan adoption used by replay;
the returned complete record is never persisted by this API.

Completed-evidence publication is immutable and idempotent. If restart reopens
a crash after the historical completed-evidence record became durable but
before receipt sealing, the core validates that record, publishes equivalent
evidence bound to the newly acquired lease projection, and reconfirms the
completed-evidence durability domain before sealing. This does not replay an
active or rejected application effect.

# RESULTS

**application_receipt** records operation kind, request, plan, attempt, target,
execution control, state projection, outcome, recovery state, six-domain
durability, normalized path consequences, optional journal, optional completed
evidence, and backend evidence.

**completed_application_evidence** exists only after every selected effect,
required synchronization, and final observation has succeeded. Failed,
partially recovered, durability-unconfirmed, or indeterminate attempts cannot
be converted into completed evidence.

**encode_application_receipt()** emits one checksummed canonical record for a
validated terminal receipt. **decode_application_receipt()** requires the exact
immutable installation, upgrade, or removal request and rebuilds the receipt
through the public factories. Completed receipts embed the existing completed-
evidence record. Decoding performs no journal access, target observation,
application replay, recovery, or state publication.

Expected stale or physical outcomes are values. Invalid construction, corrupt
canonical records, backend contract violations, and early I/O failures that
prevent truthful receipt construction are exceptions.

# THREAD SAFETY

Immutable public values support concurrent read access. One application
transaction is single-threaded unless its concrete backend documents stronger
behavior. The abstract **pkgimage::package_archive** contract does not promise
concurrent replay.

# VERSION

Version 4.0.1 exposes API version 4. Generation 4 separates journal
persistence from mutation backends; release 4.0.1 additionally exposes owner-side
rehydration for controller inspection without changing durable protocol or SONAME. Canonical durable protocol generations remain
independent of the project version and shared-library SONAME.

# SEE ALSO

**libpkgapply-posix**(3), **pkgapply**(7), **libpkgbuild-plan**(3), **libpkgbuild-image**(3), **libpkgplan**(3)
