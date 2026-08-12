# Maintaining libpkgapply

Before tagging:

1. confirm no POSIX implementation, system header, or backend storage layout
   remains in the core repository;
2. build the exact published dependency chain in GCC and Clang shared/static,
   release, and ASan/UBSan configurations;
3. run all unit, integration, protocol, header, contract, and installed-
   consumer qualification;
4. inspect pkg-config against the public semantic owners and `DT_NEEDED` against
   the reviewed implementation dependency closure plus private `libcrypto`;
5. build the shared core with GCC and Clang and require its exact dynamic
   symbol set to match `abi/libpkgapply.exports`; review any manifest change
   against the SONAME/API policy in `docs/abi.md`;
6. lint manuals and strict Doxygen, and stage-install all documentation;
7. audit integration against the corresponding `libpkgapply-posix` and
   `libpkgstate-apply` release candidates; require durable application journals
   to retain the exact admitted state-projection body and forbid recovery code
   from reconstructing that historical projection from a current state read;
8. replay the mailbox independently and compare Git trees.

The 3.0 ABI gate is closed only while the reviewed manifest, SONAME 3, and API
generation 3 remain exact in every release candidate.
