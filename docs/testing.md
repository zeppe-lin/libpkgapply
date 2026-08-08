# Testing

Core qualification is divided by evidence role instead of one flat model/engine
namespace:

- `unit` proves immutable semantic values and local invariants;
- `integration` proves request admission, backend binding, payload/application
  composition, restart authority, and one complete application vertical;
- `protocol` proves canonical journal, receipt, checkpoint, and completed-
  evidence encodings independently of the application vertical;
- `header` compiles every installed public header independently; and
- `contract` checks architecture, dependency, documentation, ABI, release,
  repository, layout, and style rules.

The composed vertical remains intentionally broad: it proves that the focused
pieces still compose.  It is not used as a substitute for focused admission,
restart, or durable-protocol qualification.

Backend transaction evidence is always checked against the immutable target
context admitted by the request.  Tests deliberately use a provider whose
reported identity/capability profile changes between callbacks and require the
core to reject that drift before accepting transaction or restart evidence.

Mechanism-provider tests do not belong in this repository. Shared and static
builds must qualify every public header and an installed consumer through
pkg-config. The Doxygen AST test receives the complete public-header dependency
include closure explicitly, so a globally installed zoo cannot hide a missing
dependency edge. GCC and Clang debug/release plus ASan/UBSan jobs are required.

The architecture contract rejects POSIX implementation paths and state
publication dependencies. The release contract also treats exact core ABI
capture as an explicit pre-tag gate until the reviewed manifest exists.
