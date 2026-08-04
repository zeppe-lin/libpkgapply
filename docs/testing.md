# Testing

Core qualification uses deterministic model tests and a scripted backend to
prove admission, precondition refusal, schedule ordering, write-ahead journal
transitions, failure classification, restart admission, forward replay,
recovery, final observation, completed evidence, and durable codecs.

Mechanism-provider tests do not belong in this repository. Shared and static
builds must qualify every public header and an installed consumer through
pkg-config. GCC and Clang debug/release plus ASan/UBSan jobs are required.

The architecture contract rejects POSIX implementation paths and state
publication dependencies. The release contract also treats exact core ABI
capture as an explicit pre-tag gate until the reviewed manifest exists.
