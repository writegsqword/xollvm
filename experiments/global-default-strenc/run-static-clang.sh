#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/xollvm/clang" >&2
  exit 2
fi

compiler=$1
case "$compiler" in
  /*) ;;
  *) compiler=$(cd "$(dirname "$compiler")" && pwd)/$(basename "$compiler") ;;
esac
compilerxx=$(dirname "$compiler")/clang++
test -x "$compiler"
test -x "$compilerxx"

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xollvm-static-clang.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

config='constenc(prob=20,maxSites=32,minAbs=4,encFP=0), mba(preset=light,prob=10,maxSites=16,maxDepth=1), adec(prob=60,maxSites=96,strength=0,ibr=1,ibrProb=70,callObfuscation=1,callProb=45,asm=0,stack=0,decoy=0,alias=0,fakeLoop=0,rdtsc=0,constLaunder=0), strenc(storage=global,cipher=aes,minlen=6,keysplit=1)'

obf_compile() {
  report=$1
  shift
  "$compiler" "$@" \
    -mllvm -enable-obfuscation \
    "-mllvm=-obf-default-config=$config" \
    -mllvm -obf-seed=7 \
    -mllvm -obf-deterministic \
    -mllvm -obf-verify \
    -mllvm "-obf-report-json=$report"
}

obf_compile "$work/defaults.json" -O2 -fno-inline \
  "$here/defaults.c" -o "$work/defaults"
"$work/defaults"
objdump -d "$work/defaults" >"$work/defaults.disassembly"
grep -Eq 'jmpq?[[:space:]]+\*' "$work/defaults.disassembly"
grep -Eq 'callq?[[:space:]]+\*' "$work/defaults.disassembly"

"$compilerxx" -O1 "$here/static_lifetime.cpp" -o "$work/static_lifetime" \
  -mllvm -enable-obfuscation \
  "-mllvm=-obf-default-config=$config" \
  -mllvm -obf-seed=11 -mllvm -obf-deterministic -mllvm -obf-verify \
  -mllvm "-obf-report-json=$work/static.json"
"$work/static_lifetime"

"$compilerxx" -O1 "$here/fork_lifetime.cpp" -o "$work/fork_lifetime" \
  -mllvm -enable-obfuscation \
  "-mllvm=-obf-default-config=$config" \
  -mllvm -obf-seed=13 -mllvm -obf-deterministic -mllvm -obf-verify \
  -mllvm "-obf-report-json=$work/fork.json"
"$work/fork_lifetime"

if "$compiler" -O1 -c "$here/annotation_rejected.c" \
    -o "$work/annotation_rejected.o" \
    -mllvm -enable-obfuscation \
    "-mllvm=-obf-default-config=$config" \
    >"$work/annotation_rejected.out" 2>&1; then
  echo "source obf: annotation unexpectedly overrode the uniform default" >&2
  exit 1
fi
grep -q 'source obf: annotation conflicts with uniform' \
  "$work/annotation_rejected.out"

obf_compile "$work/inline_asm.json" -O1 \
  "$here/inline_asm.c" -o "$work/inline_asm"
"$work/inline_asm"
grep -q '"skip_reason": "contains_inline_asm"' "$work/inline_asm.json"

for source in component component_main; do
  "$compilerxx" -O1 -fPIC -c "$here/$source.cpp" \
    -o "$work/$source.o" \
    -mllvm -enable-obfuscation \
    "-mllvm=-obf-default-config=$config" \
    -mllvm -obf-seed=17 -mllvm -obf-deterministic -mllvm -obf-verify \
    -mllvm "-obf-report-json=$work/$source.json"
done

"$compilerxx" "$work/component_main.o" "$work/component.o" -o "$work/multi_tu"
"$work/multi_tu"
"$compilerxx" -shared "$work/component.o" -o "$work/libcomponent.so"
"$compilerxx" "$work/component_main.o" -L"$work" -lcomponent \
  -Wl,-rpath,'$ORIGIN' -o "$work/shared_boundary"
"$work/shared_boundary"

for artifact in "$work/static_lifetime" "$work/fork_lifetime" \
    "$work/multi_tu" "$work/libcomponent.so" "$work/shared_boundary"; do
  if strings "$artifact" | grep -Eq \
      'xollvm-(static|fork|component)-lifetime-sentinel|xollvm-component-sentinel'; then
    echo "plaintext sentinel remains in $artifact" >&2
    exit 1
  fi
done

echo "static-Clang global-default, indirect-control-flow, and string-lifetime experiment passed"
