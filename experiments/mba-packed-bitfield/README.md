# MBA packed-bitfield regression

This reproduces the function name, input shape, transform seed, and pre-fix
34-to-121 instruction MBA selection observed when V8's
`EncodedExternalReference` constructor produced a malformed context snapshot.
It also exercises the high-preset SLE path.

The constructor performs successive packed-bitfield read/modify/write updates.
Before every field has been initialized, the loaded storage word can contain
`undef` bits. MBA identities duplicate their operands, so each operand must be
frozen once before the first rewrite. Otherwise LLVM may select a different
value for each use and invalidate an identity that is correct for fixed
bit-vectors.

Run the regression with a statically integrated XOLLVM compiler:

```sh
./experiments/mba-packed-bitfield/run.sh /path/to/xollvm/clang++
```

The test checks that MBA changed the exact constructor, that a necessary
`freeze i32` remains in optimized IR, and that both boolean values preserve the
tag, one-bit API selector, and 23-bit index for several initial storage patterns.
