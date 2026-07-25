libpkgapply
===========

`libpkgapply` is the Zeppe-Lin C++17 library for applying one accepted
`libpkgplan` package-operation plan to one managed target.

Its authority is deliberately narrow:

```text
libpkgplan    immutable intended transition
libpkgapply   observed application effects and recovery evidence
libpkgstate   durable installed truth and state publication
pkgman        transaction composition and final resolution
```

Implemented foundation
----------------------

The current development tree provides the immutable and live-authority
foundation required before target mutation is admitted:

* strict domain-separated SHA-256 application identities;
* immutable target application contexts and execution controls;
* operation-specific installation, upgrade, and removal requests retaining the
  exact accepted `libpkgplan` objects;
* complete lease-bound installed-state projections;
* rich completed filesystem-object facts with explicit known, unknown, and
  not-applicable fields;
* path consequences bound back to exact plan decisions and incoming image entry
  identifiers;
* typed application outcomes, recovery states, durability profiles, receipts,
  and publication-eligible completed evidence; and
* validation of caller-held target mutation leases against the target context,
  exclusion domain, acquisition instance, and state projection; and
* constrained backend mechanism contracts for exact observations, private
  payload staging, old-object capture, active and rejected effects, recovery,
  durability synchronization, and journal publication; and
* exact admission checks binding plan schema and kind, target, backend,
  capabilities, outer lease, installed snapshot, ownership inventory, path
  owner closure, archive bytes, image, inspection receipt, and image entry IDs;
  and
* fresh filesystem-precondition checks where every planning-time fact that was
  known must remain known and equal, while additional current facts are retained
  as richer evidence rather than rejected as drift; and
* an internal non-virtual engine admission gate that opens one backend
  transaction, validates its target, lease, backend and capability bindings,
  derives one nonce-bound attempt, performs exactly one fresh observation, and
  returns a truthful precondition-refused receipt without staging, journaling,
  capture, or mutation; and
* exact incoming regular-payload closure derivation, including deduplicated
  hard-link anchors and separate active versus rejected consumers; and
* pre-mutation old-object capture derivation that merges rejected staging and
  selected recovery needs without admitting absent or retained paths; and
* deterministic safe mechanism schedules that place old-object capture
  before mutation, regular payload staging in archive order, rejected
  publication before destructive active effects, incoming directories before
  descendants, child removals before parent cleanup, hard-link anchors before
  links, and final observation after all planned effects; and
* a private non-virtual semantic engine that durably freezes the complete
  effect graph, captures old objects, replays private incoming payloads,
  publishes source-bound rejected-object records, executes the accepted active
  effects, and retains the live transaction for recovery; and
* reverse recovery of the completed or potentially indeterminate active-effect
  prefix, with exact, partial, disabled, and indeterminate recovery truth; and
* final result observation, publication-eligibility checks, durable completed-
  evidence publication, terminal journal resolution, and immutable success or
  failure receipts; and
* public installation, upgrade, and removal `apply()` overloads that drive one
  admitted backend transaction through preparation, rejected publication,
  active effects, final observation, and automatic typed recovery before
  returning a terminal receipt.

The core now owns strict versioned byte encodings for validated journal
snapshots and durable restart checkpoints. Journal decoding revalidates
identities, bounds all input, and enforces monotonic successors. Checkpoint
decoding verifies a body checksum and binds every reconstructed replay fact to
the exact journal snapshot and immutable application request.

`libpkgapply-posix` stores both protocols in FD-anchored directories. Journal
snapshots use private temporary files, file synchronization, atomic replacement,
and directory synchronization. Checkpoints are immutable per journal-record
identity and use link-without-replace publication. Exact republication is
idempotent; stale, rewritten, foreign, conflicting, corrupt, symlinked, or
non-regular material is rejected.

The POSIX layer also provides a read-only target observer anchored to an open
root directory. It refuses symbolic-link parents, inspects leaf links without
following them, hashes regular bytes from stable descriptors, and verifies only
explicitly requested hard-link relations.

Private incoming regular payloads can now be staged beneath a retained
namespace descriptor. One application-attempt identity owns one private stage;
an immutable binding fixes the exact package image and selected regular entries
before replay. Each payload is size- and SHA-256-verified before publication,
and a sealed marker becomes restart authority only after all selected files are
synchronized. Exact replay of an already sealed stage verifies bytes without
rewriting them.

Admitted old objects can likewise be preserved in an attempt-bound private
capture namespace before any active-target mutation. Regular bytes are streamed
from a descriptor-stable source into synchronized immutable payload files;
metadata-only objects retain the exact admitted observation. A capture record is
published last, so incomplete bytes never become rejected or recovery authority.
Hard-link recovery is claimed exact only when the admitted anchor is physically
proved, or when the source has no additional links.

Rejected consequences can now be published into an attempt-scoped immutable
store. One binding admits exactly one accepted operation-plan identity for the
full application attempt, while incoming and old records occupy separate source
namespaces. Each record retains the exact rejected-effect request, logical path,
typed object facts, completeness, and provenance.

Incoming metadata-only objects are published directly from the exact package
image. Incoming regular and hard-link records additionally require the matching
sealed payload authority; hard links preserve their anchor relation and retain
self-contained verified bytes. Old records consume only captures made for
rejected publication before active mutation and never reread the target path.
Exact republication is idempotent. Record visibility and rejected-store
durability remain separate facts established by separate operations.

The POSIX layer now includes a private attempt-bound active-namespace session.
It binds one target-root descriptor, application attempt, exact incoming image,
sealed payload authority, admitted observations, and recovery captures. Incoming
objects are prepared under deterministic names in their destination parent;
regular payloads are verified before rename, hard links retain the exact anchor
inode, removal is non-recursive, and recovery restores displaced old objects or
exact captures without inventing rollback. After a terminal journal makes
recovery unnecessary, the session can discard only the attempt's displaced old
leaves. Visibility, cleanup, and active-namespace durability remain distinct
operations.

This session is deliberately not installed as another public controller. A
complete POSIX `application_backend_transaction` still has to compose it with
the outer lease, journals, restart checkpoints, rejected publication, final
observation, completed-evidence storage, and durable-attempt reopening. The core
can already classify a validated durable journal, skip completed forward work,
and divert unresolved actuator intents away from repeated actuation. Installed-
state publication remains outside this repository stage.

Authority boundary
------------------

The completed library will:

* consume an accepted installation, upgrade, or removal plan;
* require one caller-held target mutation lease;
* revalidate state and filesystem preconditions under that lease;
* stage exact incoming payloads through `libpkgimage`;
* execute only effects selected by the accepted plan;
* journal mutation attempts and recovery material;
* return an immutable application receipt; and
* return completed application evidence only after complete success.

The library will not:

* select packages or solve dependencies;
* parse package-manager configuration;
* reinterpret preserve, reject, omit, or ownership policy;
* discover or reopen package archives by pathname;
* execute unbound lifecycle declarations;
* construct `libpkgstate` installed objects;
* publish installed state; or
* claim global filesystem and state atomicity.

Requirements
------------

Build-time requirements are:

* a C++17 compiler;
* Meson 1.2.0 or later;
* Ninja;
* pkg-config;
* `libpkgimage` 0.3.0 or later;
* `libpkgplan` 0.1.0 or later; and
* OpenSSL `libcrypto` with SHA-256 EVP support.

Building
--------

Shared library:

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Static library with static dependencies:

```sh
meson setup build-static \
  -Ddefault_library=static \
  -Dlink_mode=static
meson compile -C build-static
meson test -C build-static --print-errorlogs
```

`default_library=both` is intentionally unsupported. Shared and static
artifacts are qualified as separate builds with matching dependency linkage.

Development doctrine
--------------------

Version 0.1.0 is being built contract-first. Effectful code is admitted only
with direct failure, stale-state, journal, recovery, and identity tests.

See `DESIGN.md` for the normative authority and sequencing model and
`TESTING.md` for release qualification.
