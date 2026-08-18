# ABI policy

The generation-4 development line advances the semantic core from SONAME 3 /
API generation 3 to SONAME 4 / API generation 4. Semantic journal persistence
is no longer a virtual operation of `application_backend_transaction`; the
package-manager-facing `apply()` overloads now require an explicit
`application_journal_store`, and `resume_application()` is addressed by that
store plus one immutable declaration identity rather than a caller-supplied
complete journal snapshot. The removed virtual changes transaction vtable
layout and all six application/restart entry-point manglings change, so SONAME 3
cannot truthfully describe the new binary contract. No compatibility shim is
retained.

The reviewed generation-4 ELF manifest contains 770 symbols. Relative to the
intermediate append-only generation-4 tree, the provider-authored checkpoint
aggregate/codec and transaction checkpoint callbacks are removed, while the
owner-derived `application_restart_view`, view-bearing backend reopen callbacks,
and attempt-bound reopen validator become the reviewed public seam. The private
`detail::application_journal_history` and restart-view builder remain hidden. A
shared build must export exactly the reviewed manifest under SONAME 4.

## Generation 3 lineage

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
770 symbols. The library builds with hidden default visibility, explicit public
annotations, and an ELF linker export script generated from that manifest.
`tests/contracts/check_abi_surface.sh` compares the linked shared object against
the manifest exactly. The manifest includes all five out-of-line public member functions for each of
the 30 `typed_digest<Domain>` specializations: `canonical_domain()`, `parse()`,
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

Generation-4 note: the immutable journal declaration retains the complete
admitted `lease_bound_state_projection` body and owner-authored replay seed.
Complete `application_journal_record` values remain public semantic values for
validation and pure classification, but persistence never transports complete
records. Restart derives `application_restart_view` from owner history; the view
has no codec, and provider-authored checkpoint state is absent from the core ABI.
The reviewed generation-4 ELF surface contains 770 symbols.
