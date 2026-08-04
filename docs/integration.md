# Integration

An orchestrator constructs an immutable package application request from
planner, build, source-projection, and image authority. It selects a mechanism
provider, acquires a target-scoped lease, and calls the public application
facade.

The returned evidence is complete application truth but is not installed-state
publication. `libpkgstate-apply` separately combines completed evidence with a
lease-bound state snapshot and constructs one stale-safe state publication
request.

Current public dependency floors are `libpkgplan >= 0.3.0`, `libpkgbuild >=
2.0.0`, `libpkgsource-plan >= 1.0.0`, and `libpkgimage >= 0.4.0`. OpenSSL is a
private implementation provider.
