# libpkgapply

`libpkgapply` is the semantic application authority for one accepted package
operation plan.

It validates immutable application requests, binds one exact target context and
caller-held mutation lease, revalidates planning preconditions, derives the
ordered effect schedule, records write-ahead intent and completion, classifies
recovery obligations, validates final observations, and seals completed or
non-completed application evidence.

## Owned authority

The core owns:

- application attempts and backend identities;
- immutable package application requests;
- target contexts and lease-bound state projections;
- precondition validation and deterministic effect schedules;
- rejected, active, recovery, and final-observation evidence models;
- durable journal, restart-checkpoint, completed-evidence, and application-
  receipt codecs;
- restart admission and replay policy; and
- abstract backend, transaction, observation, staging, capture, publication,
  and mutation contracts.

The core never opens host paths, acquires an OS lock, stages bytes in a
filesystem, mutates a target namespace, or chooses persistence directories.
Those mechanisms are supplied by an independent backend product.

## Repository boundary

`libpkgapply-posix` owns the extracted descriptor-anchored POSIX
implementation formerly shipped in this repository. It implements target
observation, target mutation leases, private payload and capture stores,
rejected-object publication, journal and checkpoint storage, completed-evidence
storage, active namespace mutation and recovery, and concrete backend
composition.

```text
libpkgbuild-image + libpkgsource-plan + libpkgplan
                         |
                         v
                   libpkgbuild-plan
             planner-ready built package authority
                         |
                         v
                    libpkgapply
        semantic request, schedule, journal, recovery, evidence
                         |
                         v
               application_backend contract
                         |
              +----------+----------+
              |                     |
              v                     v
      libpkgapply-posix       another mechanism provider
```

Installed state publication is not part of application. `libpkgstate-apply`
consumes completed application evidence and constructs state publication
requests after application returns.

## Version 3.0

Release 3.0 separates the already distinct POSIX product and corrects
application admission against the current authority graph. Installed headers
require:

- `libpkgbuild-plan >= 1.0.0, < 2.0.0`; and
- `libpkgplan >= 0.3.0, < 1.0.0`.

Build/image agreement and source-to-planner projection are upstream statements
retained through `libpkgbuild-plan`; they are not reconstructed by
`libpkgapply`. OpenSSL `libcrypto` is the only private pkg-config requirement.
Transitive authority libraries remain legitimate implementation ELF needs where
opaque accessors are used and enter consumer flags only for static linkage.

The 3.0 core is SONAME 3 / public API generation 3. The generation advances
because incoming package authority now admits the opaque `libpkgbuild-plan`
projection rather than the published 2.x build-result/image pair. The old ABI
is not carried as a compatibility shim.

Fallback subprojects are intentionally unsupported. Shared and static closures
must be built separately.

## Build

```sh
meson setup build \
  -Ddefault_library=shared \
  -Dlink_mode=shared \
  -Dwerror=true
meson compile -C build
meson test -C build --print-errorlogs
```

See `DESIGN.md`, `TESTING.md`, `docs/architecture.md`, `docs/integration.md`,
`docs/abi.md`, and `MAINTAINING.md` before changing the boundary.

## License

GPL-3.0-or-later. See `COPYING` and `COPYRIGHT`.
