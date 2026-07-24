Maintaining libpkgapply
=======================

Release gate
------------

A release requires:

* a clean shared and static dependency stack;
* GCC and Clang qualification;
* optimized `NDEBUG` qualification;
* address and undefined-behavior sanitizers;
* public-header isolation;
* installed `libpkgapply` and `libpkgapply-posix` pkg-config consumers;
* journal and recovery failure injection;
* state-seam qualification without a core `libpkgstate` dependency;
* generated and linted manuals;
* warning-clean Doxygen; and
* agreement among project version, API constants, SOVERSION, changelog,
  canonical schemas, journal formats, and installed metadata.

Schema discipline
-----------------

Application identities, completed evidence, journal byte encodings, and
backend storage are protocols. Do not change field order, integer width, enum tags, domain labels,
normalization, or digest framing under an existing schema version.

Patch releases must preserve public API and ABI, identity schemas, journal
readability, outcome meanings, and completed-evidence eligibility. An
incompatible change requires an explicit schema and SOVERSION transition.

Dependency direction
--------------------

Core public dependencies are `libpkgplan` and `libpkgimage` only.
`libpkgstate` may appear only in a non-installed qualification target until a
destination-owned state adapter exists in the `libpkgstate` repository.

The core must not acquire direct `libarchive`, package-manager configuration,
source-format, dependency-solver, lifecycle-policy, or state-storage
authority.

Release review
--------------

Before tagging, inspect the complete commit range since the previous release.
For each new effect or error branch, verify that the tests establish:

* no mutation before successful revalidation and staging;
* write-ahead journal ordering;
* truthful recovery and durability classification;
* final observation before completed evidence;
* no out-of-plan mutation; and
* no unsupported atomicity claim.

A green build obtained by weakening a contract test is not releasable.
