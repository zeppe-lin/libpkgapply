# ABI policy

Release 3.0 preserves core SONAME 2 and public API generation 2. Repository
separation removes the independently named POSIX product but does not by itself
change the core semantic ABI.

The existing core predates the house export-manifest discipline. Before a 3.0.0
tag, maintainers must build the extracted dependency closure, capture the exact
GCC and Clang shared-library exports, add explicit public export annotations,
apply hidden-by-default visibility and a reviewed linker manifest, and compare
the result with the 2.3.0 ABI. No symbol inventory is asserted here without a
native linkable build.

Pkg-config exposes the semantic owners present in installed headers and keeps
`libcrypto` private. Changes to public value layouts, exception hierarchies,
virtual interfaces, dependency placement, or SONAME require explicit ABI review.
