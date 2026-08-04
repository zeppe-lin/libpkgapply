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

`libpkgapply-posix` 3.0.0 owns the extracted descriptor-anchored POSIX
implementation formerly shipped in this repository. It implements target
observation, target mutation leases, private payload and capture stores,
rejected-object publication, journal and checkpoint storage, completed-evidence
storage, active namespace mutation and recovery, and concrete backend
composition.

```text
libpkgplan + libpkgbuild + libpkgsource-plan + libpkgimage
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
dependency placement against the current authority graph. Installed headers
require:

- `libpkgplan >= 0.3.0`;
- `libpkgbuild >= 2.0.0`; and
- `libpkgimage >= 0.4.0`.

`libpkgsource-plan >= 1.0.0` and OpenSSL `libcrypto` are private
implementation requirements. They remain direct shared-library dependencies
where used and enter consumer flags only for static linkage.

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
