#!/usr/bin/env bash

set -ue
set -o pipefail

script_dir=$(cd `dirname ${BASH_SOURCE[0]}`; pwd -P)

NPROC=$(sysctl -n hw.ncpu)
# NPROC_LIMIT defaults to 8. Export it to control the limit
export NPROC=`awk "BEGIN { limit=${NPROC_LIMIT:-8}; print (${NPROC} < limit) ? ${NPROC} : limit ;}"`
echo "Running with -j${NPROC}"

CMAKE_DIR=cmake-3.7.2-Darwin-x86_64
CMAKE_URL="https://cmake.org/files/v3.7/cmake-3.7.2-Darwin-x86_64.tar.gz"
CMAKE_BIN_DIR="CMake.app/Contents/bin"

. ${script_dir}/../install.sh
. ${script_dir}/../checkout.sh

make -j${NPROC} --no-print-directory -C ./llvm.build

make -j${NPROC} --no-print-directory -C ./llvm.build --keep-going \
  check-checkedc check-clang

$LNT_VE_DIR/bin/lnt runtest nt \
  --sandbox ./llvm.lnt.sandbox \
  --cc ./llvm.build/bin/clang \
  --test-suite ./llvm-test-suite \
  --cflags -fcheckedc-extension \
  -j 1 \
  --build-threads ${NPROC}

set +ue
set +o pipefail
