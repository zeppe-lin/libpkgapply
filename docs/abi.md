# ABI policy

Release 3.0 advances the semantic core from SONAME 2 / API generation 2 to
SONAME 3 / API generation 3.

This is an intentional ABI boundary, not a repository-split side effect. The
2.3.0 public `incoming_package_authority` admitted a `pkgbuild::build_result`
and `pkgimage::inspected_package_image` directly and retained those authorities
by value. The 3.0 authority correction admits one opaque
`pkgbuild::plan_adapter::artifact_projection`, retains it behind an immutable
implementation object, and exposes the accepted projection and its upstream
authorities. That changes both public entry points and layouts of public values
that retain incoming authority. The old admission entry point is not restored as
a compatibility shim; 2.x remains the compatible boundary for the old
toolchain.

The pre-tag ABI gate is closed by `abi/libpkgapply.exports`. It contains the
exact compiler-stable ELF surface reviewed from GCC and Clang shared builds:
790 symbols. The library builds with hidden default visibility, explicit public
annotations, and an ELF linker export script generated from that manifest.
`tests/contracts/check_abi_surface.sh` compares the linked shared object against
the manifest exactly. The manifest includes all five out-of-line public member functions for each of
the 27 `typed_digest<Domain>` specializations: `canonical_domain()`, `parse()`,
`algorithm()`, `string()`, and `bytes()`. Their `extern template` declarations
suppress consumer-side instantiation, so omitting any of those 150 symbols would
make ordinary installed identity consumers unlinkable. Internal canonical-record and application
engine helpers remain hidden and are linked into the protocol/integration tests
that intentionally inspect those implementation boundaries. Any exported-symbol
addition, removal, SONAME change, or public value-layout change requires explicit
ABI review.

The 2.3.0 comparison was performed against the immutable source and its
historical dependency headers. It confirmed the generation change directly:
the old `incoming_package_authority::admit(build_result,
inspected_package_image)` symbol disappears and the new
`admit(artifact_projection)` boundary appears, together with the corresponding
representation operations. Therefore retaining SONAME 2 would falsely claim
binary compatibility.

Public exception and abstract-interface vtables are anchored by out-of-line
owner definitions. This prevents consumer-side RTTI/vtable ownership from
becoming part of the ABI accidentally.

Pkg-config exposes only the semantic owners present in installed headers:
`libpkgbuild-plan` and `libpkgplan`. It keeps `libcrypto` private. Transitive
build, build-image, source-projection, source, and image libraries may remain
direct ELF needs of the implementation where opaque projection accessors are
called; they are not public compile dependencies. Changes to public value
layouts, exception hierarchies, virtual interfaces, dependency placement, or
SONAME require explicit ABI review.

Current development ABI note: application journals retain the complete admitted
`lease_bound_state_projection` body. `application_journal_header::make()` now
requires that body and `admitted_state_projection()` exposes it for recovery.
The previous naked projection-identity constructor signature is intentionally
absent; this is a reviewed development ABI break, not a compatibility surface.
