# Global defaults and static-lifetime string encryption

This project isolates two changes from any Chromium build:

1. `-obf-default-config=<spec>` supplies one baseline configuration to every
   defined IR function. Source annotations are optional and, when present,
   overlay the baseline. The intended Chromium integration uses no source
   annotations.
2. `strenc(storage=global,cipher=aes)` encrypts the initializer of the original
   string global and decrypts the same storage from a priority-zero module
   constructor. It is an experimental alternative to the legacy per-call stack
   buffers.

The test checks unannotated-function selection, indirect branches/calls,
constructor ordering, returned-pointer lifetime, pointer identity, a pointer
stored in a global initializer, and absence of the sentinel plaintext in the
final executable.

Run it against a standalone plugin build:

```sh
./run.sh /path/to/Obfuscator.so
```

## Current design assessment

`storage=global` is the most practical first implementation for browser code:
it retains every existing reference to the original global, performs one
decrypt per string rather than per call, and needs no use rewriting. Its costs
are writable string pages, private dirty RSS in each process, an observable
startup decryptor, and constructor-order constraints. The prototype therefore
excludes thread-local strings, non-local globals, and globals assigned an
explicit section.

Other viable designs, not implemented here:

- A separate ciphertext plus static plaintext allocation avoids modifying the
  original global but requires complete rewriting of instruction and constant
  initializer users. It also doubles string storage.
- Thread-safe lazy initialization shortens plaintext residency but adds a
  guard and branch on use, complicates reentrancy, and still needs stable
  global storage.
- Temporarily making read-only pages writable retains section placement but is
  platform-specific and interacts poorly with RELRO, shared pages, and sandbox
  policy.
- Linker/post-link string-table encryption can see the final layout, but loses
  source-level type/use information and requires a platform-specific runtime
  loader hook.

The global-startup approach should not be enabled for Chromium until tests
cover multiple translation units, shared libraries, static initializer order,
`fork`, and the current Chromium LLVM revision. It also needs private skip
reporting and a decision on whether generated decrypt helpers receive the same
default function passes.
