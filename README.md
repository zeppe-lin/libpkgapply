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
  owner closure, archive bytes, image, inspection receipt, and image entry IDs.

No concrete filesystem actuator, archive replay coordinator, journal storage
backend, or POSIX backend is present yet. The public model rejects inconsistent
evidence before those effectful layers are introduced.

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
