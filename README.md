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

The library will:

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

Version 0.1.0 is being built contract-first. The initial repository contains
only the build and documentation boundary. Effectful code is admitted only
with direct failure, stale-state, journal, recovery, and identity tests.

See `DESIGN.md` for the normative authority and sequencing model and
`TESTING.md` for release qualification.
