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

Public exception and abstract-interface vtables are anchored by out-of-line
owner definitions. This prevents weak consumer-side RTTI and vtable emission
while the complete export inventory is being qualified.

Pkg-config exposes only the semantic owners present in installed headers:
`libpkgbuild-plan` and `libpkgplan`. It keeps `libcrypto` private. Transitive
build, build-image, source-projection, source, and image libraries may remain
direct ELF needs of the implementation where opaque projection accessors are
called; they are not public compile dependencies. Changes to public value
layouts, exception hierarchies, virtual interfaces, dependency placement, or
SONAME require explicit ABI review.
