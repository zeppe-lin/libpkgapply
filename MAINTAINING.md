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
5. complete the core export-annotation and exact ABI-manifest gate documented in
   `docs/abi.md`;
6. lint manuals and strict Doxygen, and stage-install all documentation;
7. audit integration against the corresponding `libpkgapply-posix` and
   `libpkgstate-apply` release candidates;
8. replay the mailbox independently and compare Git trees.

Do not tag 3.0 while the exact core ABI gate remains open.
