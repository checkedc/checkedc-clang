#!/usr/bin/env bash

# Build an installation package for clang.

function resetBeforeExit {
  set +ue
  set +o pipefail
}

function cmdsucceeded {
  resetBeforeExit
  exit 0
}

function cmdfailed {
  echo "Build installation package failed."
  resetBeforeExit
  exit 1
}

set -ue
set -o pipefail
set -x

cd ${LLVM_OBJ_DIR}

if [ "${BUILD_PACKAGE}" != "Yes" ]; then cmdsucceeded; fi

echo "Building installation package for clang"

# build it
make -j${BUILD_CPU_COUNT} package
if [ "$?" -ne "0" ]; then
   cmdfailed
fi

# Installer executable in its own directory.
mv LLVM-*.tar.Z package

cmdsucceeded
