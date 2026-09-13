#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/xollvm/clang++" >&2
  exit 2
fi

compiler=$1
test -x "$compiler"

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/xollvm-mba-packed-bitfield.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

# The first configuration reproduces the reported 34-to-121 selection shape
# before the fix. The second exercises the SLE path that amplifies operand
# duplication under the high preset.
exact_config='mba(preset=light,prob=20,maxSites=24,maxDepth=1)'
high_config='mba(preset=high,prob=100,maxSites=24,maxDepth=2)'
seed=16245535801486108713
target='_ZN2v88internal2ro24EncodedExternalReferenceC2Etbj'

run_case() {
  name=$1
  config=$2

  "$compiler" -O3 -S -emit-llvm \
    "$here/packed_bitfield.cpp" -o "$work/$name.ll" \
    -mllvm -enable-obfuscation \
    "-mllvm=-obf-default-config=$config" \
    "-mllvm=-obf-seed=$seed" \
    -mllvm -obf-deterministic \
    -mllvm -obf-verify \
    -mllvm "-obf-report-json=$work/$name.json"

  # The constructor must actually be selected, and its operand freezes must
  # survive wherever they remain semantically necessary.
  jq -e --arg target "$target" '
    any(.functions[];
        .name == $target and
        any(.passes[]; .id == "mba" and .changed == true))
  ' "$work/$name.json" >/dev/null
  sed -n "/define.*$target/,/^}/p" "$work/$name.ll" |
    grep -q 'freeze i32'

  # Recompile already-transformed IR without enabling another XOLLVM pass.
  "$compiler" "$work/$name.ll" -o "$work/$name"
  "$work/$name"
}

run_case exact "$exact_config"
run_case high "$high_config"

echo "MBA packed-bitfield regression passed"
