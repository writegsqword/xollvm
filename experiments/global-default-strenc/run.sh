#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/Obfuscator.so" >&2
  exit 2
fi

plugin=$1
case "$plugin" in
  /*) ;;
  *) plugin=$(cd "$(dirname "$plugin")" && pwd)/$(basename "$plugin") ;;
esac

test -f "$plugin"

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xollvm-global-defaults.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

clang -S -emit-llvm -O1 "$here/defaults.c" -o "$work/defaults.ll"
opt -load-pass-plugin="$plugin" -passes=obf-dump-config \
  -obf-seed=7 -obf-deterministic \
  -obf-default-config='constenc(prob=10)' \
  -disable-output "$work/defaults.ll" >"$work/defaults.config"
for function in branch_and_call leaf main; do
  grep -q "OBF-CONFIG-FN $function" "$work/defaults.config"
done
opt -load-pass-plugin="$plugin" \
  -passes=obfuscation -obf-seed=7 -obf-deterministic -obf-verify \
  -obf-default-config='adec(prob=100,maxSites=80,strength=0,ibr=1,ibrProb=100,callObfuscation=1,callProb=100,asm=0,stack=0,decoy=0,alias=0,fakeLoop=0,rdtsc=0,constLaunder=0)' \
  -S "$work/defaults.ll" -o "$work/defaults.obf.ll"

grep -q 'indirectbr' "$work/defaults.obf.ll"
grep -q 'load volatile ptr' "$work/defaults.obf.ll"
clang "$work/defaults.obf.ll" -O1 -o "$work/defaults"
"$work/defaults"

# Exercise the actual Clang extension-point integration as used by a compiler
# wrapper. The source remains annotation-free.
clang -O1 -fpass-plugin="$plugin" \
  -mllvm -enable-obfuscation \
  '-mllvm=-obf-default-config=constenc(prob=100,maxSites=16,minAbs=2,encFP=0)' \
  "$here/defaults.c" -o "$work/defaults.direct"
"$work/defaults.direct"

clang++ -S -emit-llvm -O0 -Xclang -disable-O0-optnone \
  "$here/static_lifetime.cpp" \
  -o "$work/static_lifetime.ll"
opt -load-pass-plugin="$plugin" \
  -passes=obfuscation -obf-seed=11 -obf-deterministic -obf-verify \
  -obf-default-config='strenc(cipher=aes,storage=global,minlen=8)' \
  -S "$work/static_lifetime.ll" -o "$work/static_lifetime.obf.ll"

grep -q '__strenc_global_init' "$work/static_lifetime.obf.ll"
clang++ "$work/static_lifetime.obf.ll" -O1 -o "$work/static_lifetime"
"$work/static_lifetime"

clang++ -O0 -Xclang -disable-O0-optnone -fpass-plugin="$plugin" \
  -mllvm -enable-obfuscation \
  '-mllvm=-obf-default-config=strenc(cipher=aes,storage=global,minlen=8)' \
  "$here/static_lifetime.cpp" -o "$work/static_lifetime.direct"
"$work/static_lifetime.direct"

if strings "$work/static_lifetime" | grep -q \
    'xollvm-static-lifetime-sentinel-7Y4Q2'; then
  echo "plaintext sentinel remains in transformed executable" >&2
  exit 1
fi

if strings "$work/static_lifetime.direct" | grep -q \
    'xollvm-static-lifetime-sentinel-7Y4Q2'; then
  echo "plaintext sentinel remains in direct-Clang executable" >&2
  exit 1
fi

echo "global-default and static-lifetime strenc experiment passed"
