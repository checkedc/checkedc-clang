#!/usr/bin/env bash

# Build an installation package for clang.

source ./config-vars.sh

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

if [ "${BUILD_PACKAGE}" != "Yes" ]; then
  cmdsucceeded
fi

echo "======================================================================"
echo "Building installation package for clang"
echo "======================================================================"

mkdir -p "package"
rm -rf "package/*"

# Build it
ninja package
if [ "$?" -ne "0" ]; then
  cmdfailed
fi

# Installer executable in its own directory.
mv LLVM-*.tar.Z package

cmdsucceeded
