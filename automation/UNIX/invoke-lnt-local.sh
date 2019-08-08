#!/bin/bash

CURDIR=$PWD
TEST_TARGET="X86_64;ARM"
LNT_BIN=~/mysandbox/bin/lnt

export PATH=$BUILD_DIR/llvm/bin:$PATH

if [ -z "$SRC_DIR" ]; then
  SRC_DIR=/usr/local/magrang/master/src
fi

if [ -z "$BUILD_DIR" ]; then
  BUILD_DIR=/usr/local/magrang/master/build
fi

rm -rf $BUILD_DIR/LNT-Results-Release-Linux $BUILD_DIR/LLVM-Release-Linux.obj
ln -s $BUILD_DIR/llvm $BUILD_DIR/LLVM-Release-Linux.obj

if [ ! -d $SRC_DIR/llvm-test-suite ]; then
  echo "llvm-test-suite not found. Checking out llvm-test-suite at $SRC_DIR/llvm-test-suite."
  cd $SRC_DIR
  git clone https://github.com/microsoft/checkedc-llvm-test-suite.git llvm-test-suite
fi

cd $CURDIR

BUILDCONFIGURATION=Release \
BUILD_BINARIESDIRECTORY=$BUILD_DIR \
BUILD_SOURCESDIRECTORY=$SRC_DIR \
TEST_SUITE=CheckedC_clang \
LNT=yes \
LNT_SCRIPT=$LNT_BIN \
TEST_TARGET_ARCH=$TEST_TARGET \
./build-and-test.sh
