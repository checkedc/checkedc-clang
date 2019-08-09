#!/bin/bash

if [ -z "$SRC_DIR" ]; then
  echo "Specify SRC_DIR as the dir containing LLVM sources."
  exit 1
fi

if [ -z "$BUILD_DIR" ]; then
  echo "Specify BUILD_DIR as the dir containing LLVM build."
  exit 1
fi

if [ -z "$TEST_TARGET" ]; then
  TEST_TARGET="X86_64;ARM"
fi

if [ -z "$LNT_BIN" ]; then
  LNT_BIN=~/mysandbox/bin/lnt
fi

CURDIR=$PWD
export PATH=$BUILD_DIR/llvm/bin:$PATH

rm -rf $BUILD_DIR/LNT-Results-Release-Linux $BUILD_DIR/LLVM-Release-Linux.obj
ln -s $BUILD_DIR/llvm $BUILD_DIR/LLVM-Release-Linux.obj

if [ ! -d $SRC_DIR/llvm-test-suite ]; then
  echo "Checking out llvm-test-suite at $SRC_DIR/llvm-test-suite."
  cd $SRC_DIR
  git clone https://github.com/microsoft/checkedc-llvm-test-suite.git llvm-test-suite
fi

cd $CURDIR

# Invoke lnt.
BUILDCONFIGURATION=Release \
BUILD_BINARIESDIRECTORY=$BUILD_DIR \
BUILD_SOURCESDIRECTORY=$SRC_DIR \
LNT_SCRIPT=$LNT_BIN \
TEST_TARGET_ARCH=$TEST_TARGET \
TEST_SUITE=CheckedC_clang \
LNT=yes \
RUN_LOCAL=yes \
./build-and-test.sh
