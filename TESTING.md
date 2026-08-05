Testing libpkgapply
===================

Doctrine
--------

Package application is tested as an untrusted-input, stale-state, mutation,
and recovery boundary.

Every behavioral addition requires:

1. a direct successful test;
2. a direct refusal or boundary test;
3. failure injection before and after every new mutation boundary;
4. deterministic identity and ordering tests where observable;
5. documentation in the relevant contract document; and
6. manual-page coverage when public behavior changes.

No test class may be disabled merely to obtain a release build.

Model tests
-----------

Plan-bearing tests construct accepted plans only through the released
`libpkgplan` request and planner APIs. Installation and upgrade fixtures import
one complete released `libpkgbuild-plan` artifact projection; manually invented
candidate, artifact, build, image, or manifest bindings are not valid incoming
authority. Tests must not instantiate planner-private plan, decision,
precondition, publication, or ownership-transition values, and must not
substitute constructor-shaped doubles for those APIs. At least one
qualification build uses installed released `libpkgbuild-plan` and
`libpkgplan` headers and libraries rather than local test doubles.

The immutable model suite must cover:

* independent public headers;
* strict typed identity parsing and domain separation;
* fixed canonical identity vectors;
* request and operation-kind separation;
* planner-projection retention and opaque copy/move behavior;
* exact candidate, release, artifact, manifest, archive, image, and receipt
  agreement with the accepted plan;
* rejection of same-name but foreign projections and plan facts;
* target context normalization;
* lease-bound state projection completeness;
* known, unknown, and not-applicable object facts;
* hard-link relation validation;
* outcome and durability invariants;
* receipt versus completed-evidence separation; and
* canonical order and permutation stability.

Engine admission tests
----------------------

The internal non-virtual engine gate must prove:

* static authority failure opens no backend transaction;
* transaction binding failure performs no target observation;
* one backend transaction supplies one durable attempt nonce;
* the caller-held lease is revalidated after transaction creation;
* fresh path facts are observed exactly once;
* stale facts return a precondition-refused receipt;
* refusal reports every durability domain as not attempted;
* refusal creates no journal and retains no publication-eligible evidence;
* installation and upgrade use archive-bearing transactions;
* removal uses an archive-free transaction; and
* admission performs no payload, capture, effect, recovery, synchronization,
  or journal backend operation.

Precondition tests
------------------

Precondition qualification must cover:

* target-context mismatch;
* stale installed snapshot;
* stale ownership inventory;
* missing, additional, or different owner identities;
* expected absence becoming presence;
* expected presence becoming absence;
* object-kind, mode, UID, and GID changes;
* optional fact changes;
* fresh observations containing additional facts; and
* all-or-nothing refusal before mutation.

Archive staging and schedule tests
----------------------------------

Composition with planner-ready build authority and deterministic mechanism
scheduling must
cover:

* exact archive, image, and inspection-receipt binding;
* rejection of a different but same-named archive;
* regular payload closure for active and rejected outcomes;
* hard-link anchor closure;
* archive-order replay;
* zero-length regular files;
* binary payload bytes;
* sink failure before and after partial delivery;
* changed retained source;
* content-digest mismatch;
* old-object capture before any target mutation;
* rejected-object publication after its source is staged or captured;
* incoming directory parents before descendant publication;
* child removals before conditional parent cleanup;
* regular anchors before hard-link publication;
* final observation after all active and rejected effects; and
* no target mutation before private staging completes.

Scripted backend
----------------

A deterministic scripted backend must expose every semantic backend event and
allow failure injection at each boundary:

```text
lease validation
state projection validation
path observation
archive validation
payload staging
journal creation
recovery capture
effect intent synchronization
rejected publication
rejected-record identity return
rejected-store synchronization
active mutation
effect completion synchronization
final observation
completed-evidence publication
completed-evidence synchronization
receipt sealing
```

Tests inspect event order, journal records, retained recovery material,
result classification, and resource release. Rejected-object tests prove that
incoming sources are sealed before publication, old sources are captured
before publication, completed publication retains an immutable record identity,
and rejected-store synchronization is selected by durability domain rather
than confused with earlier private-stage synchronization. Failure before the
active target boundary may contain rejected consequences, but no active effect
or recovery command. Active-effect tests prove exact incoming bindings,
conditional directory retention, per-effect write-ahead ordering, explicit
visibility versus confirmed durability, and retention of the live transaction
for a separate recovery phase after failure or indeterminate completion.
Completion tests prove exact final-observation closure, contradiction and
unknown-result recovery seams, publication-ineligible evidence failures, exact
completed-record identity return, completed-evidence durability, terminal
receipt binding, and archive-free removal completion.

Public facade tests
-------------------

The package-manager-facing `apply()` overloads must prove:

* installation and upgrade retain archive-bearing authority for the whole call;
* removal opens only the archive-free transaction form;
* one backend transaction spans admission, effects, final observation, and
  terminal journal sealing;
* stale preconditions return before journal creation or mutation;
* successful calls return completed evidence and publication-eligible paths;
* active and final-observation interruptions are recovered before return;
* no internal engine session type appears in the installed public headers; and
* transaction destruction occurs only after the terminal receipt has been
  materialized.

The scripted backend supplies ordered observation snapshots so a facade test
can represent planning-time truth followed by post-effect truth without
reaching between private engine phases.

Operation tests
---------------

Installation tests cover activation, preserve, reject, omit, compatible
sharing, structural parents, empty packages, and ownership eligibility.

Upgrade tests cover old/new authority separation, replacement, obsolete paths,
old and incoming rejected objects, shared retention, ownership transfer,
hard links, and deepest-first directory cleanup.

Removal tests cover operation without current source or artifact, absent paths,
foreign owners, final-owner objects, old-object staging, conditional directory
cleanup, and empty ownership manifests.

Filesystem object tests cover regular files, directories, symbolic links,
hard links, FIFOs, character and block device capabilities, observed sockets,
unsupported objects, metadata, and unknown facts.

Journal and recovery tests
--------------------------

Journal tests cover:

* stable schema and identity vectors;
* write-ahead ordering;
* recovery material durable before destructive intent;
* crash after intent and before effect;
* crash after effect and before completion record;
* crash after all effects and before final receipt;
* completed application pending state resolution;
* exact rollback where all required facts are established;
* refusal to claim exact rollback when facts are incomplete;
* partial and indeterminate outcomes;
* corrupted-record quarantine;
* explicit finalization and abandonment; and
* garbage collection of finalized attempts.

Restart admission and replay tests separately prove:

* journal-only classification performs no backend operation;
* preparing and durably completed active prefixes may resume forward;
* unresolved, failed, or indeterminate active intents require recovery;
* terminal and externally unresolved journals are not automatically reopened;
* a new outer lease may reopen only the original durable attempt;
* the reopened transaction reports the exact journal identity and attempt nonce;
* restart admission performs no target observation or effect replay;
* rejected restart transactions release their backend resources;
* checkpoint facts must agree with journal intents, terminal outcomes, evidence,
  and six-domain durability truth;
* a durably completed forward prefix is reconstructed without repeated active
  or rejected publication;
* an unresolved active or recovery intent is never issued a second time;
* unstarted forward and reverse effects continue in frozen schedule order;
* private staging and synchronization retries remain within the same attempt;
* final observation may be repeated and must still match the accepted plan;
* completed evidence and receipt sealing resume without inventing another
  attempt or duplicate seal intent; and
* receipt-bearing journals are terminal even when their physical state remains
  visible or indeterminate.

In-process RAII cleanup and restart replay are tested separately.

Application-receipt codec tests prove:

* completed receipts retain the exact subordinate completed-evidence identity;
* precondition refusal, pre-target failure, and durability-unconfirmed outcomes
  retain their exact recovery, durability, path, journal, and backend facts;
* canonical re-encoding is byte-for-byte stable;
* same-length corruption and truncation are rejected before admission;
* a foreign immutable application request is rejected; and
* decoding performs no journal lookup, target access, replay, recovery, or
  publication.

Mechanism-provider qualification
--------------------------------

Concrete backend mechanism suites belong to their provider repositories.
The core uses the scripted backend to prove semantic ordering, failure
classification, restart policy, and evidence closure without depending on
one host implementation.

State seam tests
----------------

The independent `libpkgstate-apply` suite owns completed-evidence to
installed-state projection. Core qualification proves only the semantic seam:

* failed, partial, and indeterminate receipts cannot become completed evidence;
* completed evidence retains every request, plan, target, path, object, and
  durability binding required by a destination adapter;
* the application layer does not construct state-owned publication values; and
* `libpkgstate` is absent from the core library, tests, and pkg-config closure.

Compiler and linkage matrix
---------------------------

Release qualification uses at least:

```text
GCC    shared
GCC    static
Clang  shared
Clang  static
```

Each normal build enables warnings as errors and runs all model, semantic,
scripted-backend, journal, and installed-consumer tests.

Additional builds include:

* optimized GCC with `NDEBUG`;
* GCC address and undefined-behavior sanitizers;
* Clang address and undefined-behavior sanitizers;
* generated manual pages with `mandoc -Tlint`; and
* Doxygen with warnings as errors.

Static installed consumers use `pkg-config --static` and must receive the
complete private dependency closure. Shared consumers must not acquire
private implementation libraries.

Coverage limitations
--------------------

A release must document any environment-limited coverage, including privileged
device creation, abrupt power-loss durability, uncommon filesystems, network
filesystems, foreign architectures, or allocator fault injection.

A missing environment is not permission to omit the semantic failure test.
The scripted backend remains responsible for deterministic qualification of
the contract.
