#!/bin/bash

# This script can be used to run LNT tests locally on a user's Linux machine.
# For detailed setup instructions see
# https://github.com/Microsoft/checkedc-llvm-test-suite/blob/master/README.md

usage() {
  echo "Usage: SRC_DIR=</path/to/llvm/src> BUILD_DIR=</path/to/llvm/build> $0"
  echo "Optional flags: TEST_TARGET=\"X86_64;ARM\" LNT_BIN=</path/to/lnt>"
  exit 1
}

CURDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

if [ -z "$SRC_DIR" ]; then
  echo "Specify SRC_DIR as the dir containing LLVM sources."
  usage
fi

if [ -z "$BUILD_DIR" ]; then
  echo "Specify BUILD_DIR as the dir containing LLVM build."
  usage
fi

if [ -z "$TEST_TARGET" ]; then
  TEST_TARGET="X86_64;ARM"
fi

if [ -z "$LNT_BIN" ]; then
  LNT_BIN=~/mysandbox/bin/lnt
fi

rm -rf $BUILD_DIR/LNT-Results-Release-Linux $BUILD_DIR/LLVM-Release-Linux.obj
ln -s $BUILD_DIR/llvm $BUILD_DIR/LLVM-Release-Linux.obj

if [ ! -d $SRC_DIR/llvm-test-suite ]; then
  echo "Checking out llvm-test-suite at $SRC_DIR/llvm-test-suite."
  cd $SRC_DIR
  git clone https://github.com/microsoft/checkedc-llvm-test-suite.git llvm-test-suite
fi

cd $CURDIR
export PATH=$BUILD_DIR/llvm/bin:$PATH

# Invoke lnt.
BUILDCONFIGURATION=Release \
BUILD_BINARIESDIRECTORY=$BUILD_DIR \
BUILD_SOURCESDIRECTORY=$SRC_DIR \
LNT_SCRIPT=$LNT_BIN \
TEST_TARGET_ARCH=$TEST_TARGET \
TEST_SUITE=CheckedC_clang \
LNT=yes \
RUN_LOCAL=yes \
BMARK=$BENCHMARK \
./build-and-test.sh
