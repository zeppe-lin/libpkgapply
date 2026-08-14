# Integration

An orchestrator constructs an immutable package application request from one
accepted `libpkgplan` operation plan and one planner-ready
`libpkgbuild-plan::artifact_projection`. It selects a mechanism provider,
acquires a target-scoped lease, and calls the public application facade.

`libpkgbuild-image` already owns agreement between the successful build result
and independently inspected package image. `libpkgbuild-plan` already owns the
source-derived candidate and planner artifact projection. `libpkgapply` imports
that complete projection and binds it to the accepted operation plan; it does
not repeat either upstream proof.

The returned evidence is complete application truth but is not installed-state
publication. `libpkgstate-apply` separately combines completed evidence with a
lease-bound state snapshot and constructs one stale-safe state publication
request.

Installed headers require:

- `libpkgbuild-plan >= 1.1.0, < 2.0.0`; and
- `libpkgplan >= 0.3.0, < 1.0.0`.

OpenSSL is the only private pkg-config requirement. Build, image, source,
source-projection, and build-image libraries remain transitive implementation
needs of the opaque planner projection; they are not direct public application
dependencies. Consumer static linkage receives the complete private closure.

Fallback subprojects are unsupported. Shared and static products are built and
qualified separately.
