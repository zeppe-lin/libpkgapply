libpkgapply
===========

`libpkgapply` is the Zeppe-Lin C++17 library for applying one accepted
`libpkgplan` package-operation plan to one managed target.

Its authority is deliberately narrow:

```text
libpkgsource  sealed package declaration and source identity
libpkgbuild   verified build result and exact artifact authority
libpkgplan    immutable intended transition
libpkgapply   observed application effects and recovery evidence
libpkgstate   durable installed truth and state publication
pkgctl        transaction composition and final resolution
```

Implemented contract
--------------------

Version 2.3.0 provides the immutable, semantic, restart, and live-mechanism
contract required to apply one accepted package operation.

Installation and upgrade begin with `incoming_package_authority`: one complete
successful `libpkgbuild` result bound to an independent `libpkgimage`
inspection and the `libpkgsource-plan` candidate projection derived from the
build request's sealed source snapshot. Request construction rejects any plan
whose release, control, artifact, image, receipt, manifest, or archive
precondition differs from that authority. Removal deliberately has no incoming
package authority.

The implemented surface includes:

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
* separate validation of caller-held mutation-lease target scope for
  observation-only recovery, and full validation against the acquisition
  instance and state projection before actuation; and
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
  returning a terminal receipt; and
* an installed descriptor-anchored POSIX backend factory whose private
  transaction composes target observation, journal and checkpoint publication,
  payload staging, capture, rejected storage, active mutation, recovery,
  durability routing, completed evidence, and exact durable restart.

The core now owns strict versioned byte encodings for validated journal
snapshots, durable restart checkpoints, completed application evidence, and
terminal application receipts. Receipt decoding requires the exact immutable
application request, verifies the whole-record checksum and receipt identity,
and reconstructs values only through the public factories. Successful receipts
embed the existing completed-evidence record rather than duplicating filesystem
evidence. Journal decoding revalidates identities, bounds all input, and
enforces monotonic successors. Checkpoint decoding binds every reconstructed
replay fact to the exact journal snapshot and immutable application request.

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

The POSIX layer now also provides the caller-owned outer target mutation lease.
The caller supplies one already-selected lock-directory descriptor; the lease
opens the lock file derived from the immutable mutation-exclusion-domain
identity, refuses final symlinks, and attempts a nonblocking exclusive lock.
Waiting, retry, and backoff remain caller policy. The coordination file is never
removed, and a held lease becomes false if its named lock authority is unlinked
or replaced. Acquisition does not observe installed state, inspect target paths,
construct an application backend, or mutate the managed target.

The core exposes two deliberately different validation gates.
`validate_target_mutation_lease_scope()` proves only that one live acquisition
protects the exact application target and shared exclusion domain; this is the
truthful authority for recovery paths that merely reread canonical state after
publication may already have advanced it. `validate_target_mutation_lease()`
additionally requires a state projection established under that same lease and
remains mandatory before application or publication actuation.

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

Completed application evidence also has an immutable FD-anchored POSIX store.
The core owns its versioned complete-body encoding; the store publishes a
mode-0600 identity-keyed record without replacement and synchronizes its
namespace separately. Restart checkpoints remain replay machinery and cannot
substitute for this terminal application proof.

`application_posix_backend::from_directory_fds()` is the installed POSIX
composition boundary. It duplicates one selected target-root descriptor and the
journal, checkpoint, payload, capture, rejected, and completed-evidence store
descriptors. Fresh transactions issue one nonce without effects. Reopened
transactions preserve the original attempt and revalidate checkpoint claims
against the corresponding physical authority before completed work is skipped.
Installed-state publication remains outside this repository.

Authority boundary
------------------

The library:

* admit an exact successful native build and independently inspected image;
* consume an accepted installation, upgrade, or removal plan;
* require one caller-held target mutation lease;
* revalidate state and filesystem preconditions under that lease;
* stage exact incoming payloads through `libpkgimage`;
* execute only effects selected by the accepted plan;
* journal mutation attempts and recovery material;
* return an immutable application receipt; and
* return completed application evidence only after complete success.

The library does not:

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
* `libpkgbuild` 2.0.0 or later;
* `libpkgsource-plan` 2.0.0 or later;
* `libpkgimage` 0.3.0 or later;
* `libpkgplan` 0.2.0 or later; and
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

Installed manuals are *libpkgapply*(3), *libpkgapply-posix*(3), and
*pkgapply*(7). Set `-Dman_pages=enabled` to require `scdoc` during a build.

Release discipline
------------------

Version 2.3.0 is qualified as a native build-bound application authority. Effectful code remains admissible
only with direct failure, stale-state, journal, restart, recovery, durability,
and identity tests.

Version 0.1.0 requests and durable attempts do not contain native build
authority. Complete or recover them with the 0.1.0 libraries before upgrading;
there is no in-place journal or checkpoint migration.

See `DESIGN.md` for the normative authority and sequencing model and
`TESTING.md` for release qualification.
