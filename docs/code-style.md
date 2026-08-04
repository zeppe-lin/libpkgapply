# Code style

Use C++17, two-space indentation, no tabs, explicit ownership, immutable public
values, typed errors, and `[[nodiscard]]` for value-producing validation and
execution operations. Diagnostic text must never drive control flow.

The semantic core may express capabilities and evidence, but must not encode a
specific filesystem, process, lock, or persistence mechanism. Format C++ with
clang-format 17 using the checked-in configuration.
