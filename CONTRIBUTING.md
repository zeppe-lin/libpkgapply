Contributing to libpkgapply
===========================

Changes are reviewed as authority and failure-boundary changes, not merely as
filesystem code.

Before proposing a patch:

* identify the immutable input authority;
* state the exact effect or evidence produced;
* identify every mutation and durability boundary;
* state what happens on failure before and after each boundary;
* preserve planner, application, state, and orchestration separation;
* add direct success, refusal, failure-injection, and identity tests; and
* update the relevant design and manual contracts.

Do not add hidden discovery of configuration, package state, archives, source
trees, lifecycle programs, or target facts. Do not broaden a backend primitive
until the non-virtual semantic engine can constrain it.

Public headers use C++17, compile independently, and include their direct
dependencies. Source and build files carry REUSE-compatible SPDX headers.

Commit messages use a terse subsystem prefix, for example:

```text
model: add typed application outcomes
apply: revalidate path preconditions
journal: record write-ahead effect boundaries
```

Keep each commit buildable and testable. A behavioral implementation commit
contains the tests that establish its invariant.
