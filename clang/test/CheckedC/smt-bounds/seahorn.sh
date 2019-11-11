#!/bin/bash

rm -f *.o *.bc

inp=$1
N=${inp%%.c}
inp_bc=$N.bc
inp_bc_pp=$N.pp.bc
inp_bc_pp_ms=$N.pp.ms.bc
inp_bc_pp_ms_o=$N.pp.ms.o.bc


$P/clang -c -emit-llvm -D__SEAHORN__ -finject-verifier-calls ${@:2} -O0 -Xclang -disable-llvm-optzns -fgnu89-inline -m64 -I/mnt/f/saeed/seahorn/build/run/include -o $inp_bc $inp
if [ ! $? -eq 0 ]; then
    echo "compile error! aborting verification!"
    exit 1
fi


$S/seapp -o $inp_bc_pp --simplifycfg-sink-common=false --strip-extern=false --promote-assumptions=false --kill-vaarg=true $inp_bc
$S/seapp --simplifycfg-sink-common=false -o $inp_bc_pp_ms --horn-mixed-sem --ms-reduce-main $inp_bc_pp
$S/seaopt -f -funit-at-a-time --simplifycfg-sink-common=false -o $inp_bc_pp_ms_o -O3 --enable-indvar=false --enable-loop-idiom=false --enable-nondet-init=false --unroll-threshold=150 --disable-loop-vectorization=true --disable-slp-vectorization=true $inp_bc_pp_ms
$S/seahorn --keep-shadows=true --horn-sea-dsa --sea-dsa=ci --horn-solve -horn-inter-proc -horn-sem-lvl=mem --horn-step=large $inp_bc_pp_ms_o



