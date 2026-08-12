# Architecture

`libpkgapply` owns semantic application, not host mutation mechanics.

The input boundary is an accepted operation plan plus one exact
`libpkgbuild-plan` artifact projection. Upstream `libpkgbuild-image` owns
build/image agreement and `libpkgbuild-plan` owns candidate and artifact
projection. The output boundary is typed application evidence: completed,
precondition-refused, failed before active mutation, recovery-required,
recovered, or durability-unconfirmed.

The core selects and orders effects, but all observations and effects cross the
abstract `application_backend` and `application_backend_transaction`
interfaces. A mechanism provider owns target access, storage, synchronization,
and system-call failures.

Forbidden core dependencies include `libpkgstate`, `libpkgstate-*`, POSIX
headers, filesystem APIs, lock-file conventions, and backend storage layouts.
`libpkgapply-posix` is the reference mechanism provider and depends inward on
the core; the core does not depend outward on it.

## Backend authority

The immutable `application_target_context` is the admitted backend authority for
one application request.  A provider is revalidated when its transaction is
accepted, and transaction mutation identity, observation identity, and
capability evidence must equal the identities and capability profile already
sealed into that target context.  Restart journal backend identity is compared
to the same request-bound target authority, never to a fresh provider callback.
A provider therefore cannot change the meaning of an admitted request by
reporting a different identity or capability profile later in the call.
