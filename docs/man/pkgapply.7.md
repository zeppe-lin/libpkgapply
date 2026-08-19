% PKGAPPLY(7) libpkgapply | Version 4.0.1

# NAME

pkgapply - package application authority and sequencing semantics

# DESCRIPTION

Package application is the physical transition between one accepted
**libpkgplan** operation plan and later **libpkgstate** publication. The authority
chain is:

```
libpkgsource       sealed package declaration and source identity
libpkgbuild        successful build and exact artifact authority
libpkgbuild-image  admitted build and inspected-image agreement
libpkgbuild-plan   planner-ready candidate and artifact projection
libpkgplan         immutable intended transition
libpkgapply        observed application effects and recovery evidence
libpkgstate        durable installed truth and state publication
pkgctl             transaction composition and final resolution
```

No layer may silently absorb the authority of another.

# ADMISSION

Application begins under one caller-held mutation lease. One acquisition
identity binds the exact target context and mutation-exclusion domain to a
mechanism-issued nonce. The reference POSIX mechanism takes a nonblocking
exclusive lock in an explicit caller-selected coordination directory; it does
not select waiting, retry, or backoff policy. The exact target context, state
projection, installed snapshot, ownership inventory, path-owner closure,
planning-time filesystem facts, backend identities, and capability profile are
revalidated before mutation.

Recovery that only rereads canonical state uses the weaker target-scope lease
validator and must perform no mutation. Application and publication require the
stronger validator binding that same live acquisition to an exact state
projection. A stale pre-publication projection is never reconstructed after the
canonical state has advanced.

Installation and upgrade additionally require one admitted incoming package
imported from **libpkgbuild-plan**. Upstream **libpkgbuild-image** has already
proved build/image agreement, and the planner adapter has already projected the
source-derived candidate and artifact facts. The accepted plan must name the
same archive, image, receipt, release, candidate, artifact, manifest,
precondition, and publication before mutation. An archive pathname or package
filename is never authority. Removal requires no incoming package or archive.

A stale fact returns a precondition-refused receipt before payload staging,
journal declaration, capture, rejected publication, active mutation, or
recovery.

# EFFECT ORDER

The semantic engine freezes a deterministic mechanism schedule and publishes
one immutable journal declaration before the first mechanism effect. Thereafter
each semantic transition is one immutable journal step followed by a bounded
cursor advance:

. observe the exact precondition path closure;
. stage and seal selected incoming regular payloads under write-ahead intent;
. capture old objects required for rejection or recovery;
. publish structured rejected objects from sealed incoming or captured old authority;
. execute active effects in safe parent, child, and hard-link order;
. synchronize selected physical durability domains;
. observe the complete final path closure;
. publish and synchronize completed application evidence; and
. append the terminal receipt/completed-evidence binding.

The journal store is separate from the mutation transaction. A successful step
and cursor commit establishes journal durability; no mutation-backend journal
synchronization operation exists.

# REJECTED OBJECTS

Rejected objects carry planner-supplied source side and typed reason. Incoming
records bind candidate release, artifact, manifest, image, entry, and
observation identities. Old records bind installed release, package, installed
control, and observations. The actuator consumes these facts directly and does
not infer rejection provenance from path role or outcome combinations.

# RECOVERY

Old objects needed for recovery are captured before destructive mutation.
Recovery walks the completed or potentially indeterminate active prefix in
reverse. Exact restoration is claimed only when complete physical authority
exists. Missing, stale, contradictory, or incomplete recovery facts produce
partial or indeterminate truth rather than an invented rollback claim.

Restart never allocates another attempt nonce. The core loads the immutable
declaration and exact committed step sequence from the journal store, validates
the predecessor chain, and probes only the exact next sequence for a crash-
orphaned successor. Completed prefixes are revalidated and retained, unresolved
active or recovery intents are not blindly reissued, and unstarted work
continues in the frozen schedule. Current target observation is never used to
invent historical journal state.

A controller that must classify retained progress without immediately replaying
it uses **rehydrate_application_journal()**. The core, not the controller or
mechanism provider, validates the exact append-only chain and returns only a
derived in-memory record suitable for pure restart assessment.

# RESULT AND STATE SEAM

Application receipts have an owner-defined canonical byte encoding. Decode
requires the original immutable application request and does not infer request,
plan, target, or control authority from digest text. A successful receipt
reuses the completed-evidence encoding; failed receipts remain a distinct
schema branch and cannot become publication authority.

A completed receipt contains publication-eligible path consequences and
completed application evidence. That evidence is application truth, not
installed-state truth. **libpkgapply** does not construct installed packages,
installed control, state deltas, publication requests, or publication receipts.
A destination-owned state adapter must consume the request-bound source and
build authority, accepted plan, completed application evidence, and prior state.
It may not accept a second caller-supplied build-provenance value.

# NON-GOALS

Version 4.0.1 does not solve dependencies, select packages, parse policy,
execute lifecycle declarations, discover archives, publish installed state, or
claim global filesystem/state atomicity.

# SEE ALSO

**libpkgapply**(3), **libpkgapply-posix**(3), **libpkgbuild-plan**(3), **libpkgbuild-image**(3), **libpkgplan**(3)
