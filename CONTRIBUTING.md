# Contributing

Preserve the semantic application boundary in `DESIGN.md`.

Core changes may define immutable requests, evidence, scheduling, journals,
restart policy, recovery obligations, and abstract backend contracts. They must
not open host paths, call filesystem mutation APIs, choose storage layouts,
acquire operating-system locks, publish installed state, or execute lifecycle
programs.

For every semantic change, identify authority inputs, accepted/refused states,
write-ahead transitions, active-mutation boundary, recovery consequence,
terminal evidence, identity/codec effects, ABI consequences, and tests.
Diagnostic strings are not control flow. Keep patches single-purpose and pass
`git diff --check`.
